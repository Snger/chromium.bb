// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_CHROMEOS_POWER_POWER_STATUS_H_
#define ASH_SYSTEM_CHROMEOS_POWER_POWER_STATUS_H_

#include "ash/ash_export.h"
#include "base/basictypes.h"
#include "base/observer_list.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "chromeos/dbus/power_manager_client.h"
#include "chromeos/dbus/power_supply_status.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {
namespace internal {

// PowerStatus is a singleton that receives updates about the system's
// power status from chromeos::PowerManagerClient and makes the information
// available to interested classes within Ash.
class ASH_EXPORT PowerStatus : public chromeos::PowerManagerClient::Observer {
 public:
  // Different styles of battery icons.
  enum IconSet {
    ICON_LIGHT,
    ICON_DARK
  };

  // Interface for classes that wish to be notified when the power status
  // has changed.
  class Observer {
   public:
    // Called when the power status changes.
    virtual void OnPowerStatusChanged() = 0;

   protected:
    virtual ~Observer() {}
  };

  virtual ~PowerStatus();

  // Sets the global instance. Must be called before any calls to Get().
  static void Initialize();

  // Destroys the global instance.
  static void Shutdown();

  // Returns true if the global instance is initialized.
  static bool IsInitialized();

  // Gets the global instance. Initialize must be called first.
  static PowerStatus* Get();

  void set_status_for_testing(const chromeos::PowerSupplyStatus& status) {
    status_ = status;
  }

  // Adds or removes an observer.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Requests updated status from the power manager.
  void RequestStatusUpdate();

  // Returns true if a battery is present.
  bool IsBatteryPresent() const;

  // Returns true if the battery is full.
  bool IsBatteryFull() const;

  // Returns the battery's remaining charge as a value in the range [0.0,
  // 100.0].
  double GetBatteryPercent() const;

  // Returns the battery's remaining charge, rounded to an integer with a
  // maximum value of 100.
  int GetRoundedBatteryPercent() const;

  // Returns true if the battery's time-to-full and time-to-empty estimates
  // should not be displayed because the power manager is still calculating
  // them.
  bool IsBatteryTimeBeingCalculated() const;

  // Returns the estimated time until the battery is empty (if line power
  // is disconnected) or full (if line power is connected). These estimates
  // should only be used if IsBatteryTimeBeingCalculated() returns false.
  base::TimeDelta GetBatteryTimeToEmpty() const;
  base::TimeDelta GetBatteryTimeToFull() const;

  // Returns true if line power (including a charger of any type) is connected.
  bool IsLinePowerConnected() const;

  // Returns true if an official, non-USB charger is connected.
  bool IsMainsChargerConnected() const;

  // Returns true if a USB charger (which is likely to only support a low
  // charging rate) is connected.
  bool IsUsbChargerConnected() const;

  // Returns the image that should be shown for the battery's current state.
  gfx::ImageSkia GetBatteryImage(IconSet icon_set) const;

  // Returns an string describing the current state for accessibility.
  base::string16 GetAccessibleNameString() const;

 protected:
  PowerStatus();

 private:
  // Overriden from PowerManagerClient::Observer.
  virtual void PowerChanged(const chromeos::PowerSupplyStatus& status) OVERRIDE;

  ObserverList<Observer> observers_;

  // Current state.
  chromeos::PowerSupplyStatus status_;

  DISALLOW_COPY_AND_ASSIGN(PowerStatus);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_SYSTEM_CHROMEOS_POWER_POWER_STATUS_H_
