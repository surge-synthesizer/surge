#pragma once

template <int dt> class AllpassFilter
{
  public:
    AllpassFilter()
    {
        wpos = 0;
        setA(0.3);
        for (int i = 0; i < dt; i++)
            buffer[i] = 0.f;
    }
    float process(float x)
    {
        wpos = (wpos + 1) % dt;
        float y = buffer[wpos];
        buffer[wpos] = y * -a + x;
        return y + buffer[wpos] * a;
    }
    void setA(float a) { this->a = a; }

  protected:
    float buffer[dt];
    float a;
    int wpos;
};
