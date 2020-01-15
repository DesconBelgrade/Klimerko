// Arduino Moving Average Library
// https://github.com/JChristensen/movingAvg
// Copyright (C) 2018 by Jack Christensen and licensed under
// GNU GPL v3.0, https://www.gnu.org/licenses/gpl.html

#ifndef MOVINGAVG_H_INCLUDED
#define MOVINGAVG_H_INCLUDED

class movingAvg
{
    public:
        movingAvg(int interval)
            : m_interval(interval), m_nbrReadings(0), m_sum(0), m_next(0) {}
        void begin();
        int reading(int newReading);
        int getAvg();
        void reset();

    private:
        int m_interval;     // number of data points for the moving average
        int m_nbrReadings;  // number of readings
        long m_sum;         // sum of the m_readings array
        int m_next;         // index to the next reading
        int *m_readings;    // pointer to the dynamically allocated interval array
};
#endif
