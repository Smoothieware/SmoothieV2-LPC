#include "Conveyor.h"

#include "AxisDefns.h"
#include "GCode.h"
#include "Block.h"
#include "Planner.h"
#include "ConfigReader.h"
#include "StepTicker.h"
#include "Robot.h"
#include "StepperMotor.h"
#include "PlannerQueue.h"
#include "main.h"

#include "FreeRTOS.h"
#include "task.h"

#include <functional>
#include <vector>

#define queue_delay_time_ms_key "queue_delay_time_ms"

#define PQUEUE (Planner::getInstance()->queue)

// TODO move ramfunc define to a utils.h
#define _ramfunc_ __attribute__ ((section(".ramfunctions"),long_call,noinline))

/*
 * The conveyor manages the planner queue, and starting the executing chain of blocks
 */
Conveyor *Conveyor::instance= nullptr;

Conveyor::Conveyor() : Module("conveyor")
{
    if(instance == nullptr) instance= this;
    running = false;
    allow_fetch = false;
    flush= false;
    halted= false;
}

bool Conveyor::configure(ConfigReader& cr)
{
    // Attach to the end_of_move stepper event
    ConfigReader::section_map_t m;
    if(cr.get_section("conveyor", m)) {
        queue_delay_time_ms = cr.get_int(m, queue_delay_time_ms_key, 100);
    }
    return true;
}

// called when everything has been configured
void Conveyor::start()
{
    //StepTicker.getInstance()->finished_fnc = std::bind( &Conveyor::all_moves_finished, this);
    running = true;
}

// this maybe called in Timer context so we cannot wait for queue to flush
void Conveyor::on_halt(bool flg)
{
    halted= flg;

    if(flg) {
        flush= true;
    }
}

// see if we are idle
// this checks the block queue is empty, and that the step queue is empty and
// checks that all motors are no longer moving
bool Conveyor::is_idle() const
{
    if(PQUEUE->empty()) {
        for(auto &a : Robot::getInstance()->actuators) {
            if(a->is_moving()) return false;
        }
        return true;
    }

    return false;
}

// Wait for the queue to be empty and for all the jobs to finish in step ticker
// This must be called in the command thread context and will stall the command thread
void Conveyor::wait_for_idle(bool wait_for_motors)
{
    // wait for the job queue to empty, forcing stepticker to run them
    while (!PQUEUE->empty()) {
        check_queue(true); // forces queue to be made available to stepticker
        safe_sleep(10); // is 10ms ok?
    }

    if(wait_for_motors) {
        // now we wait for all motors to stop moving
        while(!is_idle()) {
            safe_sleep(10); // is 10ms ok?
        }
    }

    // returning now means that everything has totally finished
}

void Conveyor::wait_for_room()
{
    while (PQUEUE->full()) {
        safe_sleep(10); // is 10ms ok?
    }
}

#define TICKS2MS( xTicks ) ( (uint32_t) ( (xTicks * 1000) / configTICK_RATE_HZ ) )
// should be called when idle, it is called when the command loop runs
void Conveyor::check_queue(bool force)
{
    static TickType_t last_time_check = xTaskGetTickCount();
    // don't check if we are not running
    if(!force && !running) return;

    if(PQUEUE->empty()) {
        allow_fetch = false;
        last_time_check = xTaskGetTickCount(); // reset timeout
        return;
    }

    // if we have been waiting for more than the required waiting time and the queue is not empty, or the queue is full, then allow stepticker to get the tail
    // we do this to allow an idle system to pre load the queue a bit so the first few blocks run smoothly.
    if(force || PQUEUE->full() || (TICKS2MS(xTaskGetTickCount() - last_time_check) >= queue_delay_time_ms) ) {
        last_time_check = xTaskGetTickCount(); // reset timeout
        if(!flush) allow_fetch = true;
        return;
    }
}

// called from step ticker ISR
// we only ever access or change the read/tail index of the queue so this is thread safe
_ramfunc_ bool Conveyor::get_next_block(Block **block)
{
    // empty the entire queue
    if (flush){
        while (!PQUEUE->empty()) {
            PQUEUE->release_tail();
        }
        flush= false;
        return false;
    }

    // default the feerate to zero if there is no block available
    this->current_feedrate= 0;

    if(halted || PQUEUE->empty()) return false; // we do not have anything to give

    // wait for queue to fill up, optimizes planning
    if(!allow_fetch) return false;

    Block *b= PQUEUE->get_tail();
    //assert(b != nullptr);
    // we cannot use this now if it is being updated
    if(!b->locked) {
        //assert(b->is_ready); // should never happen

        b->is_ticking= true;
        b->recalculate_flag= false;
        this->current_feedrate= b->nominal_speed;
        *block= b;
        return true;
    }

    return false;
}

// called from step ticker ISR when block is finished, do not do anything slow here
_ramfunc_ void Conveyor::block_finished()
{
    // release the tail
    PQUEUE->release_tail();
}

/*
    In most cases this will not totally flush the queue, as when streaming
    gcode there is one stalled waiting for space in the queue, in
    queue_head_block() so after this flush, once main_loop runs again one more
    gcode gets stuck in the queue, this is bad. Current work around is to call
    this when the queue in not full and streaming has stopped
*/
void Conveyor::flush_queue()
{
    allow_fetch = false;
    flush= true;

    // TODO force deceleration of last block

    // now wait until the block queue has been flushed
    wait_for_idle(false);

    flush= false;
}

// Debug function
// Probably not thread safe
// only call within command thread context or not while planner is running
void Conveyor::dump_queue()
{
    // start the iteration at the head
    PQUEUE->start_iteration();
    Block *b = PQUEUE->get_head();
    int i= 0;
    while (!PQUEUE->is_at_tail()) {
        printf("block %03d > ", ++i);
        b->debug();
        b= PQUEUE->tailward_get(); // walk towards the tail
    }
}
