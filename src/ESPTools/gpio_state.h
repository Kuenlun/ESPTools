#pragma once

#include "ESPTools/core.h"

namespace ESPTools
{

  /**
   * @brief Handles GPIO states (Undefined, Low, and High). It is essentially an enum encapsulated
   * within a struct, allowing the use of methods to work with the enum.
   */
  struct GpioState
  {
    enum Value : int8_t
    {
      Undefined = -1,
      Low = 0,
      High = 1
    };

    /**
     * @brief Initializes the state to "Undefined" if no parameters are provided to the constructor
     */
    constexpr GpioState() : value_(Value::Undefined) {}

    /**
     * @brief Allows direct initialization from functions such as `gpio_get_level()`
     *
     * @tparam T Type of the input state value, deduced automatically from the input
     * @param state Input state value to use for initializing the GpioState object
     * @param inverse_logic If set to true, the input `state` logic will be inverted
     */
    template <typename T>
    constexpr GpioState(const T state, const bool inverse_logic = false)
        : value_((inverse_logic) ? (static_cast<Value>(!static_cast<bool>(state)))
                                 : (static_cast<Value>(static_cast<bool>(state))))
    {
    }

    /**
     * @brief Allows switch and comparisons
     */
    constexpr operator Value() const { return value_; }

    /**
     * @brief Deleted bool operator to prevent usage like `if (state)`
     */
    explicit operator bool() const = delete;

    /**
     * @brief Overloads the negation operator to invert the GpioState
     *
     * @details
     * - If the state is Low, it changes it to High.
     * - If the state is High, it changes it to Low.
     * - If the state is Undefined, it remains Undefined.
     *
     * @return A new GpioState object representing the inverted state.
     */
    constexpr GpioState operator!() const
    {
      switch (value_)
      {
      case Value::Low:
        return Value::High;
      case Value::High:
        return Value::Low;
      default:
        return Value::Undefined;
      }
    }

    /**
     * @brief Provides a human-readable string representation for the GpioState
     *
     * @return A C-style string
     */
    constexpr const char *ToStr() const
    {
      switch (value_)
      {
      case Value::Low:
        return "Low";
      case Value::High:
        return "High";
      default:
        return "Undefined";
      }
    }

  private:
    Value value_;
  };

} // namespace ESPTools
