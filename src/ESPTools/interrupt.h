#pragma once

#include "ESPTools/core.h"
#include "ESPTools/gpio_state.h"

#include <driver/gpio.h>

#include <freertos/semphr.h>
#include <freertos/timers.h>

namespace ESPTools
{

  class Interrupt
  {
  public:
    // Tag for the Interrupt class
    static constexpr char LOG_TAG[]{ESPTOOLS_LOG_TAG_CREATOR("Interrupt")};

    /**
     * @brief Saves the GPIO configuration, configures the GPIO, adds the ISR handler for the
     * specific GPIO pin, and creates a counting semaphore for tracking interrupts.
     *
     * @param gpio_num GPIO pin number associated with the interrupt
     * @param low_to_high_time_ticks
     * @param high_to_low_time_ticks
     * @param gpio_mode GPIO mode: set input/output mode
     * @param pull_up GPIO pull-up
     * @param pull_down GPIO pull-down
     * @param inverse_logic Flag indicating whether the logic should be inverted or not
     */
    Interrupt(const gpio_num_t gpio_num,
              const TickType_t low_to_high_time_ticks = 0,
              const TickType_t high_to_low_time_ticks = 0,
              const gpio_mode_t gpio_mode = GPIO_MODE_INPUT,
              const gpio_pullup_t pull_up = GPIO_PULLUP_DISABLE,
              const gpio_pulldown_t pull_down = GPIO_PULLDOWN_DISABLE,
              const bool inverse_logic = false);

    /**
     * @brief Detaches the interrupt, restores the GPIO configuration to its default state,
     * and releases the memory used by the FreeRTOS objects.
     */
    ~Interrupt();

    /**
     * @brief Deleted copy constructor
     */
    Interrupt(const Interrupt &other) = delete;

    /**
     * @brief Deleted move constructor
     */
    Interrupt(Interrupt &&other) = delete;

    /**
     * @brief Deleted copy assignment
     */
    Interrupt &operator=(const Interrupt &other) = delete;

    /**
     * @brief Deleted move assignment
     */
    Interrupt &operator=(Interrupt &&other) = delete;

    /**
     * @brief Blocks the calling task until an interrupt occurs or the specified block time expires.
     * When an interrupt is processed, the interrupt counting semaphore decreases by one.
     *
     * @param xBlockTime Time in ticks to wait for the semaphore to become available
     * @return pdTRUE if an interrupt has occurred, pdFALSE if the block time has expired
     */
    BaseType_t WaitForSingleInterrupt(const TickType_t xBlockTime = portMAX_DELAY) const;

    /**
     * @brief Blocks the calling task until an interrupt occurs or the specified block time expires.
     * When an interrupt is processed, the interrupt counting semaphore decreases to 0 or 1 so
     * that the pin state alternates between high and low while minimizing very quick interrupts.
     * Suitable for cases where rapid interrupts (e.g. ringing or bouncing) can be discarded.
     *
     * @param xBlockTime Time in ticks to wait for the semaphore to become available
     * @return pdTRUE if an interrupt has occurred, pdFALSE if the block time has expired
     */
    BaseType_t WaitForLastInterrupt(const TickType_t xBlockTime = portMAX_DELAY) const;

    /**
     * @return UBaseType_t Number of pending interrupts
     */
    inline UBaseType_t PendingInterrupts() const
    {
      return uxSemaphoreGetCount(interrupt_counting_semaphore_);
    }

    /**
     * @return UBaseType_t Number of redundant interrupts
     * Interrupts that if removed do not create gaps in interrupt states
     */
    inline UBaseType_t RedundantInterrupts() const
    {
      const UBaseType_t pending_interrupts{PendingInterrupts()};
      return pending_interrupts - (pending_interrupts % 2);
    }

    /**
     * @return ---------------------------------------------------------------------------------------------------------------
     */
    inline GpioState LastState() const { return fsm_state_; };

    /**
     * @brief Get the current state of the interrupt considering pending interrupts.
     * It calculates the number of unprocessed interrupts and determines the current interrupt
     * state by taking pending interrupts into account.
     *
     * @return Current state of the interrupt considering pending interrupts
     */
    GpioState State() const;

  public: // Getters
    /**
     * @return GPIO number associated with the interrupt
     */
    inline gpio_num_t GpioNum() const { return gpio_num_; }

    /**
     * @return Raw state of the interrupt
     */
    inline GpioState RawState() const { return raw_state_; }

    /**
     * @return Bool value representing if the logic is inverted or not
     */
    inline bool InverseLogic() const { return inverse_logic_; }

  public: // Setters
    /**
     * @return TickType_t& ---------------------------------------------------------------------------------------------------------------
     */
    inline TickType_t &GoHighTime() { return low_to_high_time_ticks_; }

    /**
     * @return TickType_t& ---------------------------------------------------------------------------------------------------------------
     */
    inline TickType_t &GoLowTime() { return high_to_low_time_ticks_; }

  private: // Private methods
    /**
     * @brief Checks if the state of the interrupt represented by `RawState` is different
     * from the already filtered interrupt state. Only processes the interrupt if the states differ.
     * ---------------------------------------------------------------------------------------------------------------
     */
    void Debouncer(const GpioState new_state);

    /**
     * @brief ---------------------------------------------------------------------------------------------------------------
     */
    void ProcessInterrupt(const GpioState new_state);

    /**
     * @brief ---------------------------------------------------------------------------------------------------------------
     */
    void FsmTransition(const GpioState new_state);

    /**
     * @brief ---------------------------------------------------------------------------------------------------------------
     */
    void FsmReset(const GpioState new_state);

    /**
     * @brief ---------------------------------------------------------------------------------------------------------------
     */
    void StateChanger();

    /**
     * @brief ---------------------------------------------------------------------------------------------------------------
     */
    void AssignIdStoreInMapsAndCreateDaemonTask();

    /**
     * @brief ---------------------------------------------------------------------------------------------------------------
     */
    void FreeIdUnmapAndDeleteDaemonTask();

  private: // Private static functions
    /**
     * @brief Static ISR function for the Interrupt class that handles external
     * interrupt events from GPIO pins.
     * It reads the state of the interrupt pin (applaying inverted logic if inverse_logic is true),
     * increments the semaphore and yields if a higher priority task was unblocked.
     *
     * This function is shared by all instances of the class. Despite being static, it receives a
     * Interrupt object through the "arg" parameter, allowing access to class members.
     * It is not a method of the class because, as an ISR, it needs to follow a specific format:
     * "gpio_isr_t" i.e. "void (*gpio_isr_t)(void *arg)".
     *
     * @param arg Interrupt object passed as a void *const
     */
    static void IRAM_ATTR InterruptHandler(void *const arg);

    /**
     * @brief FreeRTOS task that handles interrupt processing.
     * The task continuously waits for notifications from the ISR (`InterruptHandler` function),
     * wherein the notifications are represented as bit masks of the triggered interrupt IDs.
     * The function scans each bit of the notification value, and if a bit is set, it processes
     * the associated interrupt.
     *
     * @param parameters
     */
    static void DaemonTask(void *const parameters);

    /**
     * @brief ---------------------------------------------------------------------------------------------------------------
     *
     * @param xTimer
     */
    static void TimerCallback(const TimerHandle_t xTimer);

  private: // Private members
    const gpio_num_t gpio_num_;
    const gpio_config_t pGPIOConfig_;
    const bool inverse_logic_;
    SemaphoreHandle_t interrupt_counting_semaphore_{nullptr};
    TickType_t low_to_high_time_ticks_{0};
    TickType_t high_to_low_time_ticks_{0};
    TimerHandle_t change_state_timer_;
    GpioState raw_state_;
    GpioState fsm_state_{GpioState::Undefined};
    // See if it is needed ---------------------------------------------------------------------------------------------------------------
    SemaphoreHandle_t protect_timer_semaphore_{nullptr};

  }; // class Interrupt

} // namespace ESPTools
