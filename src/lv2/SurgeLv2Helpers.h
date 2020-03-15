#pragma once

// Tracking of the caller of `sendParameterAutomated`
//  it determines which logic to use in order to handle parameter sends
enum CallerType { kCallerNone, kCallerDsp, kCallerUi };
extern thread_local CallerType callerType;

// RAII wrapper for the caller type
class ScopedCallerType {
public:
   explicit ScopedCallerType(CallerType ct) : prevCt(callerType) { callerType = ct; }
   ~ScopedCallerType() { callerType = prevCt; }
private:
   ScopedCallerType(const ScopedCallerType &) = delete;
   ScopedCallerType &operator=(const ScopedCallerType &) = delete;
private:
   CallerType prevCt;
};
