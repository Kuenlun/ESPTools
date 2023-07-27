#include "ESPTools/interrupt.h"
#include "ESPTools/logger.h"

#include <freertos/task.h>

#include <bitset>
#include <unordered_map>

namespace ESPTools
{

  // Maximum number of interrupts that can be used
  static constexpr uint8_t MAX_INTERRUPTS{32};
  // Unordered map that allows retrieval of interrupt IDs based on their GPIO number
  // The ID value is assigned based on the order in which the interrupt objects are created
  static std::unordered_map<gpio_num_t, uint8_t> gpio_to_id_map;
  // Unordered map that allows retrieval of Interrupt objects based on their ID
  static std::unordered_map<uint8_t, Interrupt *> id_to_interrupt_map;
  // BitSet to track used IDs
  static std::bitset<MAX_INTERRUPTS> used_ids;
  // Mutex used to protect the use of maps
  static SemaphoreHandle_t global_vars_mutex{nullptr};
  // Handle to the interrupt daemon task
  static TaskHandle_t interrupt_daemon_task_handle{nullptr};

  void Interrupt::TimerCallback(const TimerHandle_t xTimer)
  {
    // Create an alias for direct access to the interrupt object
    Interrupt &interrupt_obj{*static_cast<Interrupt *>(pvTimerGetTimerID(xTimer))};
    interrupt_obj.StateChanger();
  }

  void Interrupt::StateChanger()
  {
    FsmState() = !FsmState();
    xSemaphoreGive(interrupt_counting_semaphore_);
    ESPTOOLS_LOGD("(GPIO %u) State changed to %s", GpioNum(), FsmState().ToStr());
  }

  void Interrupt::FsmTransition(const GpioState new_state)
  {
    ESPTOOLS_LOGV("(GPIO %u) Changing from %s to %s",
                  GpioNum(), FsmState().ToStr(), new_state.ToStr());
    // Determine 'wait_time' based on whether we're switching to high or low state,
    // using respective rise or fall time
    const TickType_t wait_time{(new_state == GpioState::High) ? GoHighTime() : GoLowTime()};
    if (wait_time) // If `wait_time` > 0 ticks change state with using the timer
      xTimerChangePeriod(change_state_timer_, wait_time, portMAX_DELAY);
    else // If `wait_time` == 0 ticks change state directly
      StateChanger();
  }

  void Interrupt::FsmReset(const GpioState new_state)
  {
    ESPTOOLS_LOGV("(GPIO %u) Remaining in %s", GpioNum(), FsmState().ToStr());
    xTimerStop(change_state_timer_, portMAX_DELAY);
  }

  void Interrupt::ProcessInterrupt(const GpioState new_state)
  {
    // Set `FsmState()` to the opposite of `new_state` the first time
    if (FsmState() == GpioState::Undefined)
    {
      FsmState() = !new_state;
      xSemaphoreGive(interrupt_counting_semaphore_);
      return;
    }
    // If `FsmState` != `new_state` start the transition of the state
    if (FsmState() != new_state)
      FsmTransition(new_state); // Start timer: Change `FsmState()` to `new_state`
    else
      FsmReset(new_state); // Stop timer: Remain in `FsmState()`
  }

  void Interrupt::Debouncer(const GpioState new_state)
  {
    static GpioState previous_state;
    // Process the interrupt if it is different from the last one
    if (previous_state != new_state)
    {
      previous_state = new_state;
      ProcessInterrupt(new_state);
    }
    else
    {
      ESPTOOLS_LOGV("(GPIO %u) Debouncer: Got same interrupt: %s", GpioNum(), new_state.ToStr());
    }
  }

  void Interrupt::DaemonTask(void *const parameters)
  {
    ESPTOOLS_LOGI("Interrupt daemon task started");

    while (true)
    {
      // Wait for ISR notification using `ulTaskNotifyTake`.
      // `xTaskNotifyWait` is not used in order to prevent missing notifications during the
      // processing of the current one.
      // The notification value is cleared here rather than setting each bit to 0 later (when
      // processing each Interrupt) to avoid losing an interrupt that could occur after
      // processing previous interrupts but before clearing the notification value.
      // This approach ensures all interrupts are properly processed.
      std::bitset<MAX_INTERRUPTS> notify_value{ulTaskNotifyTake(pdTRUE, portMAX_DELAY)};
      //ESPTOOLS_LOGD("Got notification value: %s", notify_value.to_string().c_str());

      // Check on bits of `notify_value` to determine triggered Interrupts in ISR
      for (auto i{notify_value._Find_first()}; i < notify_value.size(); i = notify_value._Find_next(i))
      {
        Interrupt *interrupt_ptr;
        xSemaphoreTake(global_vars_mutex, portMAX_DELAY);
        {
          interrupt_ptr = id_to_interrupt_map[i];
        }
        xSemaphoreGive(global_vars_mutex);
        interrupt_ptr->Debouncer(interrupt_ptr->RawState());
      }
    }
  }

  void IRAM_ATTR Interrupt::InterruptHandler(void *const arg)
  {
    // Create an alias for direct access to the interrupt object
    Interrupt &interrupt_obj{*static_cast<Interrupt *>(arg)};

    // Get the GPIO level
    const auto raw_gpio_level{gpio_get_level(interrupt_obj.GpioNum())};
    // Save the raw state of the interrupt (invert logic if necessary)
    interrupt_obj.RawState() = GpioState(raw_gpio_level, interrupt_obj.InverseLogic());

    BaseType_t xHigherPriorityTaskWoken{pdFALSE};
    if (xSemaphoreTakeFromISR(global_vars_mutex, &xHigherPriorityTaskWoken))
    {
      // Notify `DaemonTask` that an interrupt has happened
      xTaskNotifyFromISR(interrupt_daemon_task_handle,
                         CreateBitMaskAt(gpio_to_id_map[interrupt_obj.GpioNum()]),
                         eSetBits,
                         &xHigherPriorityTaskWoken);
      xSemaphoreGiveFromISR(global_vars_mutex, &xHigherPriorityTaskWoken);
    }
    // If a higher priority task was unblocked, yield from ISR
    if (xHigherPriorityTaskWoken)
      portYIELD_FROM_ISR();
  }

  /**
   * @brief Finds an unused ID from `gpio_to_id_map`.
   * IDs are marked as `used` in a bitset. If no unused IDs exist, it returns `MAX_INTERRUPTS`.
   *
   * @return uint8_t Unused ID or `MAX_INTERRUPTS` if all are used.
   */
  static uint8_t FindFreeId()
  {
    // Iterate over each pair in the map and set the corresponding bit in used_ids
    for (const auto &kvPair : gpio_to_id_map)
      used_ids.set(kvPair.second);
    // Look for an unused ID
    for (uint8_t i{0}; i < MAX_INTERRUPTS; ++i)
    {
      // If this ID is not marked as used, return it
      if (!used_ids[i])
        return i;
    }
    // If all IDs are used, return MAX_INTERRUPTS
    return MAX_INTERRUPTS;
  }

  /**
   * @brief ------------------------------------------------------------------------------------
   */
  static void CreateGlobalVarsMutexIfNotCreated()
  {
    // Create a mutex to protect the use of the unordered maps
    if (!global_vars_mutex)
    {
      global_vars_mutex = xSemaphoreCreateMutex();
      assert(global_vars_mutex);
    }
  }

  void Interrupt::AssignIdStoreInMapsAndCreateDaemonTask()
  {
    // Create `global_vars_mutex` if not created
    CreateGlobalVarsMutexIfNotCreated();

    xSemaphoreTake(global_vars_mutex, portMAX_DELAY);
    {
      // The first time create the task daemon
      if (used_ids.none())
      {
        const BaseType_t ret{xTaskCreatePinnedToCore(DaemonTask,
                                                     "Filtered Int",
                                                     3 * 1024,
                                                     nullptr,
                                                     configTIMER_TASK_PRIORITY,
                                                     &interrupt_daemon_task_handle,
                                                     APP_CORE_ID)};
        assert(ret);
      }
      // There can only be 32 filtered interrupts at most
      assert(!used_ids.all());
      // Get a free ID
      const uint8_t id{FindFreeId()};
      // Store the id in `gpio_to_id_map`
      gpio_to_id_map[GpioNum()] = id;
      // Store the interrupt object in `id_to_interrupt_map`
      id_to_interrupt_map[id] = this;
    }
    xSemaphoreGive(global_vars_mutex);
  }

  void Interrupt::FreeIdUnmapAndDeleteDaemonTask()
  {
    xSemaphoreTake(global_vars_mutex, portMAX_DELAY);
    {
      // ------------------------- Empty maps and reset bit from bitset
      assert(false);
      // Delete the Interrupt daemon task if this is the last object
      if (used_ids.none())
        vTaskDelete(interrupt_daemon_task_handle);
    }
    xSemaphoreGive(global_vars_mutex);
  }

  Interrupt::Interrupt(const gpio_num_t gpio_num,
                       const TickType_t low_to_high_time_ticks,
                       const TickType_t high_to_low_time_ticks,
                       const gpio_mode_t gpio_mode,
                       const gpio_pullup_t pull_up,
                       const gpio_pulldown_t pull_down,
                       const bool inverse_logic)
      : gpio_num_(gpio_num),
        pGPIOConfig_{.pin_bit_mask = BIT(gpio_num),
                     .mode = gpio_mode,
                     .pull_up_en = pull_up,
                     .pull_down_en = pull_down,
                     .intr_type = GPIO_INTR_ANYEDGE},
        inverse_logic_(inverse_logic),
        low_to_high_time_ticks_(low_to_high_time_ticks),
        high_to_low_time_ticks_(high_to_low_time_ticks)
  {
    // Create a counting semaphore to protect FSM state changes inside timer callback
    protect_timer_semaphore_ = xSemaphoreCreateCounting(configTIMER_QUEUE_LENGTH, 0);
    assert(protect_timer_semaphore_);
    // Create a counting semaphore for interrupt tracking with uxMaxCount
    // set to the maximum value of UBaseType_t
    interrupt_counting_semaphore_ = xSemaphoreCreateCounting(UBASETYPE_MAX, 0);
    assert(interrupt_counting_semaphore_);

    AssignIdStoreInMapsAndCreateDaemonTask();

    // Create the timer that will be in charge of changing the state of the Interrupt
    change_state_timer_ = xTimerCreate("TimerCallback",
                                       portMAX_DELAY, // Will be changed
                                       pdFALSE,
                                       static_cast<void *>(this),
                                       TimerCallback);
    assert(change_state_timer_);

    // Configure the GPIO pin
    ESP_ERROR_CHECK(gpio_config(&pGPIOConfig_));
    // Install ISR service
    InstallIsrService(0);
    // Add ISR handler for the specific GPIO
    ESP_ERROR_CHECK(gpio_isr_handler_add(GpioNum(), InterruptHandler, this));

    ESPTOOLS_LOGI("Interrupt object created on GPIO %u", GpioNum());
  }

  Interrupt::~Interrupt()
  {
    FreeIdUnmapAndDeleteDaemonTask();

    // Delete the timer
    xTimerDelete(change_state_timer_, portMAX_DELAY);
    // Restore the GPIO configuration to its default state
    const gpio_config_t default_config{.pin_bit_mask = BIT(GpioNum()),
                                       .mode = GPIO_MODE_DISABLE,
                                       .pull_up_en = GPIO_PULLUP_DISABLE,
                                       .pull_down_en = GPIO_PULLDOWN_DISABLE,
                                       .intr_type = GPIO_INTR_DISABLE};
    ESP_ERROR_CHECK(gpio_config(&default_config));
    // Delete the counting semaphores
    vSemaphoreDelete(interrupt_counting_semaphore_);
    vSemaphoreDelete(protect_timer_semaphore_);

    ESP_LOGI(LOG_TAG, "Interrupt object destroyed on GPIO %u", GpioNum());
  }

} // namespace ESPTools
