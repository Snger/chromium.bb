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
static const int kMaxGesturingFingers = 5;
static const int kMaxTapFingers = 5;

class TapRecord {
 public:
  void Update(const HardwareState& hwstate,
              const set<short, kMaxTapFingers>& added,
              const set<short, kMaxTapFingers>& removed,
              const set<short, kMaxFingers>& dead);
  void Clear();

  // if any gesturing fingers are moving
  bool Moving(const HardwareState& hwstate) const;
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
  set<short, kMaxGesturingFingers> pointing_;

  // Tap-to-click
  // The current state:
  TapToClickState tap_to_click_state_;

  // When we entered the state:
  stime_t tap_to_click_state_entered_;

  TapRecord tap_record_;

  // General time limit for tap gestures
  stime_t tap_timeout_;

  // How long it takes to stop dragging when you let go of the touchpad.
  stime_t tap_drag_timeout_;

  // If we are currently pointing, scrolling, etc.
  GestureType current_gesture_type_;
};

}  // namespace gestures

#endif  // GESTURES_IMMEDIATE_INTERPRETER_H_
