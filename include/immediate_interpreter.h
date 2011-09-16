// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>  // for FRIEND_TEST

#include "gestures/include/gestures.h"
#include "gestures/include/interpreter.h"
#include "gestures/include/map.h"
#include "gestures/include/set.h"

#ifndef GESTURES_IMMEDIATE_INTERPRETER_H_
#define GESTURES_IMMEDIATE_INTERPRETER_H_

namespace gestures {

// This interpreter keeps some memory of the past and, for each incoming
// frame of hardware state, immediately determines the gestures to the best
// of its abilities.

// Currently it simply does very basic pointer movement.

static const int kMaxFingers = 5;
static const int kMaxGesturingFingers = 2;
static const int kMaxTapFingers = 5;

class TapRecord {
 public:
  void Update(const HardwareState& hwstate,
              const set<short, kMaxTapFingers>& added,
              const set<short, kMaxTapFingers>& removed,
              const set<short, kMaxFingers>& dead);
  void Clear();

  // if any gesturing fingers are moving
  bool Moving(const HardwareState& hwstate, const float dist_max) const;
  bool TapComplete() const;  // is a completed tap
  int TapType() const;  // return GESTURES_BUTTON_* value
 private:
  void NoteTouch(short the_id, const FingerState& fs);  // Adds to touched_
  void NoteRelease(short the_id);  // Adds to released_
  void Remove(short the_id);  // Removes from touched_ and released_

  map<short, FingerState, kMaxTapFingers> touched_;
  set<short, kMaxTapFingers> released_;
};

class ImmediateInterpreter : public Interpreter {
  FRIEND_TEST(ImmediateInterpreterTest, SameFingersTest);
  FRIEND_TEST(ImmediateInterpreterTest, PalmTest);
  FRIEND_TEST(ImmediateInterpreterTest, GetGesturingFingersTest);
  FRIEND_TEST(ImmediateInterpreterTest, TapToClickStateMachineTest);
  FRIEND_TEST(ImmediateInterpreterTest, TapToClickEnableTest);
 public:
  enum TapToClickState {
    kTtcIdle,
    kTtcFirstTapBegan,
    kTtcTapComplete,
    kTtcSubsequentTapBegan,
    kTtcDrag,
    kTtcDragRelease,
    kTtcDragRetouch
  };

  ImmediateInterpreter();
  virtual ~ImmediateInterpreter();

  virtual Gesture* SyncInterpret(HardwareState* hwstate,
                                 stime_t* timeout);

  virtual Gesture* HandleTimer(stime_t now, stime_t* timeout);

  void SetHardwareProperties(const HardwareProperties& hw_props);

  TapToClickState tap_to_click_state() const { return tap_to_click_state_; }

  // TODO(adlr): replace these with proper properties when they're available.
  void set_tap_timeout(stime_t timeout) { tap_timeout_ = timeout; }
  void set_tap_drag_timeout(stime_t timeout) { tap_drag_timeout_ = timeout; }

  virtual void Configure(GesturesPropProvider* pp, void* data);

  virtual void Deconfigure(GesturesPropProvider* pp, void* data);

 private:
  // Returns true iff the fingers in hwstate are the same ones in prev_state_
  bool SameFingers(const HardwareState& hwstate) const;

  // Reset the member variables corresponding to same-finger state and
  // updates changed_time_ to |now|.
  void ResetSameFingersState(stime_t now);

  // Updates *palm_, pointing_ below.
  void UpdatePalmState(const HardwareState& hwstate);

  // Gets the finger or fingers we should consider for gestures.
  // Currently, it fetches the (up to) two fingers closest to the keyboard
  // that are not palms. There is one exception: for t5r2 pads with > 2
  // fingers present, we return all fingers.
  set<short, kMaxGesturingFingers> GetGesturingFingers(
      const HardwareState& hwstate) const;

  // Updates current_gesture_type_ based on passed-in hwstate and
  // considering the passed in fingers as gesturing.
  void UpdateCurrentGestureType(
      const HardwareState& hwstate,
      const set<short, kMaxGesturingFingers>& gs_fingers);

  // If the fingers are near each other in location and pressure and might
  // to be part of a 2-finger action, returns true.
  bool TwoFingersGesturing(const FingerState& finger1,
                           const FingerState& finger2) const;

  // Given that TwoFingersGesturing returns true for 2 fingers,
  // This will further look to see if it's really 2 finger scroll or not.
  // Returns the current state (move or scroll) or kGestureTypeNull if
  // unknown.
  GestureType GetTwoFingerGestureType(const FingerState& finger1,
                                      const FingerState& finger2);

  const char* TapToClickStateName(TapToClickState state);

  stime_t TimeoutForTtcState(TapToClickState state);

  void SetTapToClickState(TapToClickState state,
                          stime_t now);

  void UpdateTapGesture(const HardwareState* hwstate,
                        const set<short, kMaxGesturingFingers>& gs_fingers,
                        const bool same_fingers,
                        stime_t now,
                        stime_t* timeout);

  void UpdateTapState(const HardwareState* hwstate,
                      const set<short, kMaxGesturingFingers>& gs_fingers,
                      const bool same_fingers,
                      stime_t now,
                      unsigned* buttons_down,
                      unsigned* buttons_up,
                      stime_t* timeout);

  // Does a deep copy of hwstate into prev_state_
  void SetPrevState(const HardwareState& hwstate);

  // Returns true iff finger is in the bottom, dampened zone of the pad
  bool FingerInDampenedZone(const FingerState& finger) const;

  // Called when fingers have changed to fill start_positions_.
  void FillStartPositions(const HardwareState& hwstate);

  // Updates the internal button state based on the passed in |hwstate|.
  void UpdateButtons(const HardwareState& hwstate);

  // By looking at |hwstate| and internal state, determins if a button down
  // at this time would correspond to a left/middle/right click. Returns
  // GESTURES_BUTTON_{LEFT,MIDDLE,RIGHT}.
  int EvaluateButtonType(const HardwareState& hwstate);

  // Precondition: current_mode_ is set to the mode based on |hwstate|.
  // Computes the resulting gesture, storing it in result_.
  void FillResultGesture(const HardwareState& hwstate,
                         const set<short, kMaxGesturingFingers>& fingers);

  HardwareState prev_state_;
  set<short, kMaxGesturingFingers> prev_gs_fingers_;
  HardwareProperties hw_props_;
  Gesture result_;

  // Button data
  // Which button we are going to send/have sent for the physical btn press
  int button_type_;  // left, middle, or right

  // If we have sent button down for the currently down button
  bool sent_button_down_;

  // If we haven't sent a button down by this time, send one
  stime_t button_down_timeout_;

  // When fingers change, we record the time
  stime_t changed_time_;

  // When fingers change, we keep track of where they started.
  // Map: Finger ID -> (x, y) coordinate
  map<short, std::pair<int, int>, kMaxFingers> start_positions_;

  // Same fingers state. This state is accumulated as fingers remain the same
  // and it's reset when fingers change.
  set<short, kMaxFingers> palm_;  // tracking ids of known palms
  set<short, kMaxFingers> pending_palm_;  // tracking ids of potential palms
  // tracking ids of known non-palms
  set<short, kMaxFingers> pointing_;

  // Tap-to-click
  // The current state:
  TapToClickState tap_to_click_state_;

  // When we entered the state:
  stime_t tap_to_click_state_entered_;

  TapRecord tap_record_;

  // If we are currently pointing, scrolling, etc.
  GestureType current_gesture_type_;

  // Properties

  // Is Tap-To-Click enabled
  GesturesPropBool tap_enable_;
  GesturesProp* tap_enable_prop_;
  // General time limit [s] for tap gestures
  stime_t tap_timeout_;
  GesturesProp* tap_timeout_prop_;
  // Time [s] it takes to stop dragging when you let go of the touchpad
  stime_t tap_drag_timeout_;
  GesturesProp* tap_drag_timeout_prop_;
  // Distance [mm] a finger can move and still register a tap
  double tap_move_dist_;
  GesturesProp* tap_move_dist_prop_;
  // Maximum pressure above which a finger is considered a palm
  double palm_pressure_;
  GesturesProp* palm_pressure_prop_;
  // Time [s] to block movement after number or identify of fingers change
  stime_t change_timeout_;
  GesturesProp* change_timeout_prop_;
  // Time [s] to wait before locking on to a gesture
  stime_t evaluation_timeout_;
  GesturesProp* evaluation_timeout_prop_;
  // If two fingers have a pressure difference greater than this, we assume
  // one is a thumb.
  double two_finger_pressure_diff_thresh_;
  GesturesProp* two_finger_pressure_diff_thresh_prop_;
  // Maximum distance [mm] two fingers may be separated and still be eligible
  // for a two-finger gesture (e.g., scroll / tap / click)
  double two_finger_close_distance_thresh_;
  GesturesProp* two_finger_close_distance_thresh_prop_;
  // Consider scroll vs pointing if finger moves at least this distance [mm]
  double two_finger_scroll_distance_thresh_;
  GesturesProp* two_finger_scroll_distance_thresh_prop_;
  // A finger must change in pressure by less than this amount to trigger motion
  double max_pressure_change_;
  GesturesProp* max_pressure_change_prop_;
  // During a scroll one finger determines scroll speed and direction.
  // Maximum distance [mm] the other finger can move in opposite direction
  double scroll_stationary_finger_max_distance_;
  GesturesProp* scroll_stationary_finger_max_distance_prop_;
  // Height [mm] of the bottom zone
  double bottom_zone_size_;
  GesturesProp* bottom_zone_size_prop_;
  // Time [s] to evaluate number of fingers for a click
  stime_t button_evaluation_timeout_;
  GesturesProp* button_evaluation_timeout_prop_;
};

}  // namespace gestures

#endif  // GESTURES_IMMEDIATE_INTERPRETER_H_
