#pragma once

class FpuState
{
  public:
    FpuState();

    void set();
    void restore();

    int _old_SSE_state;
    int _SSE_Flags;
};
