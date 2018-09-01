#include "PID_Autotuner.h"
#include "TemperatureControl.h"
#include "SigmaDeltaPwm.h"
#include "OutputStream.h"
#include "GCode.h"
#include "main.h"

#include <cmath>        // std::abs

//#define DEBUG_PRINTF s->printf
#define DEBUG_PRINTF(...)

PID_Autotuner::PID_Autotuner(TemperatureControl *tc) : temp_control(tc)
{
    lastInputs = nullptr;
    peaks = nullptr;
    tickCnt = 0;
    nLookBack = 10 * 20; // 10 seconds of lookback (fixed 50ms tick period, 20/sec)
}

void PID_Autotuner::start(GCode& gcode, OutputStream& os)
{
            // set target
            float target = 150.0;
            if (gcode.has_arg('S')) {
                target = gcode.get_arg('S');
                os.printf("Target: %5.1f\n", target);
            }

            // set the cycles, really not needed for this new version
            int ncycles = 8;
            if (gcode.has_arg('C')) {
                ncycles = gcode.get_arg('C');
                if(ncycles < 8) ncycles= 8;
            }

            // optionally set the noise band, default is 0.5
            if (gcode.has_arg('B')) {
                noiseBand = gcode.get_arg('B');
            }

            // optionally set the look back in seconds default is 10 seconds
            if (gcode.has_arg('L')) {
                nLookBack = gcode.get_arg('L');
            }

            os.printf("%s: Starting PID Autotune, %d max cycles, control X aborts\n", temp_control->get_designator(), ncycles);

            this->run_auto_pid(os, target, ncycles);
}

/**
 * this autopid is based on https://github.com/br3ttb/Arduino-PID-AutoTune-Library/blob/master/PID_AutoTune_v0/PID_AutoTune_v0.cpp
 */
void PID_Autotuner::run_auto_pid(OutputStream& os, float target, int ncycles)
{
    noiseBand = 0.5;
    oStep = temp_control->heater_pin->max_pwm(); // use max pwm to cycle temp
    lookBackCnt = 0;
    tickCnt = 0;

    if (lastInputs != nullptr) delete[] lastInputs;
    lastInputs = new float[nLookBack + 1];

    temp_control->heater_pin->set(false);
    temp_control->target_temperature = 0.0;

    target_temperature = target;
    requested_cycles = ncycles;

    if (peaks != nullptr) delete[] peaks;
    peaks = new float[ncycles];

    for (int i = 0; i < ncycles; i++) {
        peaks[i] = 0.0;
    }

    peakType = 0;
    peakCount = 0;
    justchanged = false;
    firstPeak= false;
    output= 0;

    // we run in a loop with a 50ms delay
    while(true) {
        safe_sleep(50);
        // TODO may want to use clock systimer to get a more accurate time
        tickCnt += 50;

        if(temp_control->is_halted()) {
            // control X breaks out
            os.printf("Autopid aborted\n");
            abort();
            return;
        }

        if(peakCount >= requested_cycles) {
            os.printf("// WARNING: Autopid did not resolve within %d cycles, these results are probably innacurate\n", requested_cycles);
            finishUp(os);
            return;
        }

        float refVal = temp_control->get_temperature();

        // oscillate the output base on the input's relation to the setpoint
        if (refVal > target_temperature + noiseBand) {
            output = 0;
            //temp_control->heater_pin->pwm(output);
            temp_control->heater_pin->set(false);
            if(!firstPeak) {
                firstPeak= true;
                absMax= refVal;
                absMin= refVal;
            }

        } else if (refVal < target_temperature - noiseBand) {
            output = oStep;
            temp_control->heater_pin->pwm(output);
        }

        if ((tickCnt % 1000) == 0) {
            os.printf("// Autopid Status - %5.1f/%5.1f @%d %d/%d\n",  refVal, target_temperature, output, peakCount, requested_cycles);
        }

        if(!firstPeak){
            // we wait until we hit the first peak before we do anything else, we need to ignore the initial warmup temperatures
            continue;
        }

        // find the peaks high and low
        bool isMax = true, isMin = true;
        for (int i = nLookBack - 1; i >= 0; i--) {
            float val = lastInputs[i];
            if (isMax) isMax = refVal > val;
            if (isMin) isMin = refVal < val;
            lastInputs[i + 1] = lastInputs[i];
        }

        lastInputs[0] = refVal;

        // we don't want to trust the maxes or mins until the inputs array has been filled
        if (lookBackCnt < nLookBack) {
            lookBackCnt++; // count number of times we have filled lastInputs
            continue;
        }

        if (isMax) {
            if(refVal > absMax) absMax= refVal;

            if (peakType == 0) peakType = 1;
            if (peakType == -1) {
                peakType = 1;
                justchanged = true;
                peak2 = peak1;
            }
            peak1 = tickCnt;
            peaks[peakCount] = refVal;

        } else if (isMin) {
            if(refVal < absMin) absMin= refVal;
            if (peakType == 0) peakType = -1;
            if (peakType == 1) {
                peakType = -1;
                peakCount++;
                justchanged = true;
            }

            if (peakCount < requested_cycles) peaks[peakCount] = refVal;
        }

        if (justchanged && peakCount >= 4) {
            // we've transitioned. check if we can autotune based on the last peaks
            float avgSeparation = (fabsf(peaks[peakCount - 1] - peaks[peakCount - 2]) + fabsf(peaks[peakCount - 2] - peaks[peakCount - 3])) / 2;
            os.printf("// Cycle %d: max: %g, min: %g, avg separation: %g\n", peakCount, absMax, absMin, avgSeparation);
            if (peakCount > 3 && avgSeparation < (0.05 * (absMax - absMin))) {
                DEBUG_PRINTF("Stabilized\n");
                finishUp(os);
                return;
            }
        }

        if ((tickCnt % 1000) == 0) {
            DEBUG_PRINTF("lookBackCnt= %d, peakCount= %d, absmax= %g, absmin= %g, peak1= %lu, peak2= %lu\n", lookBackCnt, peakCount, absMax, absMin, peak1, peak2);
        }

        justchanged = false;
    }
}


void PID_Autotuner::finishUp(OutputStream& os)
{
    //we can generate tuning parameters!
    float Ku = 4 * (2 * oStep) / ((absMax - absMin) * 3.14159);
    float Pu = (float)(peak1 - peak2) / 1000;
    os.printf("\tKu: %g, Pu: %g\n", Ku, Pu);

    float kp = 0.6 * Ku;
    float ki = 1.2 * Ku / Pu;
    float kd = Ku * Pu * 0.075;

    os.printf("\tTrying:\n\tKp: %5.1f\n\tKi: %5.3f\n\tKd: %5.0f\n", kp, ki, kd);

    temp_control->setPIDp(kp);
    temp_control->setPIDi(ki);
    temp_control->setPIDd(kd);

    os.printf("PID Autotune Complete! The settings above have been loaded into memory, but not written to your config file.\n");

    // and clean up
    abort();
}

void PID_Autotuner::abort()
{
    if (temp_control == nullptr)
        return;

    temp_control->target_temperature = 0;
    temp_control->heater_pin->set(false);
    temp_control = nullptr;

    if (peaks != nullptr)
        delete[] peaks;
    peaks = nullptr;
    if (lastInputs != nullptr)
        delete[] lastInputs;
    lastInputs = nullptr;
}
