#pragma once

#include "Module.h"

class PlannerQueue;
class Block;
class OutputStream;

class Conveyor : public Module
{
public:
    static Conveyor *getInstance() { return instance; }

    Conveyor();
    // delete copy and move constructors and assign operators
    Conveyor(Conveyor const&) = delete;             // Copy construct
    Conveyor(Conveyor&&) = delete;                  // Move construct
    Conveyor& operator=(Conveyor const&) = delete;  // Copy assign
    Conveyor& operator=(Conveyor &&) = delete;      // Move assign

    bool configure(ConfigReader& cr);
    void start();

    void on_halt(bool flg);

    void check_queue(bool force= false);

    void wait_for_idle(bool wait_for_motors=true);
    void wait_for_room();
    bool is_idle() const;

    // returns next available block writes it to block and returns true
    bool get_next_block(Block **block);
    void block_finished();
    void flush_queue(void);
    void force_queue() { check_queue(true); }

    float get_current_feedrate() const { return current_feedrate; }

    // debug function
    void dump_queue();

private:
    static Conveyor *instance;

    uint32_t queue_delay_time_ms{100};
    float current_feedrate{0}; // actual nominal feedrate that current block is running at in mm/sec

    struct {
        volatile bool running:1;
        volatile bool allow_fetch:1;
        bool flush:1;
        bool halted:1;
    };

};
