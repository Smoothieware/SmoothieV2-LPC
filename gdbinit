# GDB initialization script with commands to make debugging Smoothie-v2 more convenient.
#
# This gdbinit file is placed here in the root folder of the repository so that it can be included from the various
# locations in the source tree from which a user might want to launch rake/make and GDB.
#
# Issuing "help user-defined" command from within GDB will list the commands added by this script.
#
# Initially created by Adam Green on June 8th, 2017.


# Hooking load command to automatically reset the processor once the load is completed.
define hookpost-load
    monitor reset
end

define hook-load
    monitor reset
end

# Command to enable/disable catching of Cortex-M faults as soon as they occur.
define catchfaults
    if ($argc > 0 && $arg0==0)
        set var $catch_faults=0
        set var *(int*)0xE000EDFC &= ~0x7F0
    else
        set var $catch_faults=1
        set var *(int*)0xE000EDFC |= 0x7F0
    end
end

document catchfaults
Instructs Cortex-M processor to stop in GDB if any fault is detected.

User can pass in a parameter of 0 to disable this feature.
end


# Command to display information about an ARMv7-M fault if one is currently active in the current frame.
define showfault
    set var $ipsr_val = $xpsr & 0xF
    if ($ipsr_val >= 3 && $ipsr_val <= 6)
        # Dump Hard Fault.
        set var $fault_reg = *(unsigned int*)0xE000ED2C
        if ($fault_reg != 0)
            printf "**Hard Fault**\n"
            printf "  Status Register: 0x%08X\n", $fault_reg
            if ($fault_reg & (1 << 31))
                printf "    Debug Event\n"
            end
            if ($fault_reg & (1 << 1))
                printf "    Vector Table Read\n"
            end
            if ($fault_reg & (1 << 30))
                printf "    Forced\n"
            end
        end

        set var $cfsr_val = *(unsigned int*)0xE000ED28

        # Dump Memory Fault.
        set var $fault_reg = $cfsr_val & 0xFF
        if ($fault_reg != 0)
            printf "**MPU Fault**\n"
            printf "  Status Register: 0x%08X\n", $fault_reg
            if ($fault_reg & (1 << 7))
                printf "    Fault Address: 0x%08X\n", *(unsigned int*)0xE000ED34
            end
            if ($fault_reg & (1 << 5))
                printf "    FP Lazy Preservation\n"
            end
            if ($fault_reg & (1 << 4))
                printf "    Stacking Error\n"
            end
            if ($fault_reg & (1 << 3))
                printf "    Unstacking Error\n"
            end
            if ($fault_reg & (1 << 1))
                printf "    Data Access\n"
            end
            if ($fault_reg & (1 << 0))
                printf "    Instruction Fetch\n"
            end
        end

        # Dump Bus Fault.
        set var $fault_reg = ($cfsr_val >> 8) & 0xFF
        if ($fault_reg != 0)
            printf "**Bus Fault**\n"
            printf "  Status Register: 0x%08X\n", $fault_reg
            if ($fault_reg & (1 << 7))
                printf "    Fault Address: 0x%08X\n", *(unsigned int*)0xE000ED38
            end
            if ($fault_reg & (1 << 5))
                printf "    FP Lazy Preservation\n"
            end
            if ($fault_reg & (1 << 4))
                printf "    Stacking Error\n"
            end
            if ($fault_reg & (1 << 3))
                printf "    Unstacking Error\n"
            end
            if ($fault_reg & (1 << 2))
                printf "    Imprecise Data Access\n"
            end
            if ($fault_reg & (1 << 1))
                printf "    Precise Data Access\n"
            end
            if ($fault_reg & (1 << 0))
                printf "    Instruction Prefetch\n"
            end
        end

        # Usage Fault.
        set var $fault_reg = $cfsr_val >> 16
        if ($fault_reg != 0)
            printf "**Usage Fault**\n"
            printf "  Status Register: 0x%08X\n", $fault_reg
            if ($fault_reg & (1 << 9))
                printf "    Divide by Zero\n"
            end
            if ($fault_reg & (1 << 8))
                printf "    Unaligned Access\n"
            end
            if ($fault_reg & (1 << 3))
                printf "    Coprocessor Access\n"
            end
            if ($fault_reg & (1 << 2))
                printf "    Invalid Exception Return State\n"
            end
            if ($fault_reg & (1 << 1))
                printf "    Invalid State\n"
            end
            if ($fault_reg & (1 << 0))
                printf "    Undefined Instruction\n"
            end
        end
    else
        printf "Not currently in Cortex-M fault handler!\n"
    end

end

document showfault
Display ARMv7-M fault information if current stack frame is in a fault handler.
end


# Dumps a core dump that is compatible with CrashDebug (https://github.com/adamgreen/CrashDebug).
define gcore
    select-frame 0

    # Starts with a header that indicates this is a CrashCatcher dump file.
    dump binary value crash.dump (unsigned int)0x00034363

    # Hardcoding flags to indicate that there will be floating point registers in dump file.
    append binary value crash.dump (unsigned int)0x00000001

    # Dump the integer registers.
    append binary value crash.dump (unsigned int)$r0
    append binary value crash.dump (unsigned int)$r1
    append binary value crash.dump (unsigned int)$r2
    append binary value crash.dump (unsigned int)$r3
    append binary value crash.dump (unsigned int)$r4
    append binary value crash.dump (unsigned int)$r5
    append binary value crash.dump (unsigned int)$r6
    append binary value crash.dump (unsigned int)$r7
    append binary value crash.dump (unsigned int)$r8
    append binary value crash.dump (unsigned int)$r9
    append binary value crash.dump (unsigned int)$r10
    append binary value crash.dump (unsigned int)$r11
    append binary value crash.dump (unsigned int)$r12
    append binary value crash.dump (unsigned int)$sp
    append binary value crash.dump (unsigned int)$lr
    append binary value crash.dump (unsigned int)$pc
    append binary value crash.dump (unsigned int)$xpsr
    append binary value crash.dump (unsigned int)$msp
    append binary value crash.dump (unsigned int)$psp

    # The exception PSR and crashing PSR are one in the same.
    append binary value crash.dump (unsigned int)$xpsr

    # Dump the floating point registers
    append binary value crash.dump (float)$s0
    append binary value crash.dump (float)$s1
    append binary value crash.dump (float)$s2
    append binary value crash.dump (float)$s3
    append binary value crash.dump (float)$s4
    append binary value crash.dump (float)$s5
    append binary value crash.dump (float)$s6
    append binary value crash.dump (float)$s7
    append binary value crash.dump (float)$s8
    append binary value crash.dump (float)$s9
    append binary value crash.dump (float)$s10
    append binary value crash.dump (float)$s11
    append binary value crash.dump (float)$s12
    append binary value crash.dump (float)$s13
    append binary value crash.dump (float)$s14
    append binary value crash.dump (float)$s15
    append binary value crash.dump (float)$s16
    append binary value crash.dump (float)$s17
    append binary value crash.dump (float)$s18
    append binary value crash.dump (float)$s19
    append binary value crash.dump (float)$s20
    append binary value crash.dump (float)$s21
    append binary value crash.dump (float)$s22
    append binary value crash.dump (float)$s23
    append binary value crash.dump (float)$s24
    append binary value crash.dump (float)$s25
    append binary value crash.dump (float)$s26
    append binary value crash.dump (float)$s27
    append binary value crash.dump (float)$s28
    append binary value crash.dump (float)$s29
    append binary value crash.dump (float)$s30
    append binary value crash.dump (float)$s31
    append binary value crash.dump (unsigned int)$fpscr

    # Dump 128k of RAM starting at 0x10000000.
    #   First two words indicate memory range.
    append binary value crash.dump (unsigned int)0x10000000
    append binary value crash.dump (unsigned int)(0x10000000 + 128*1024)
    append binary memory crash.dump 0x10000000 (0x10000000 + 128*1024)

    # Dump 72k of RAM starting at 0x10080000.
    #   First two words indicate memory range.
    append binary value crash.dump (unsigned int)0x10080000
    append binary value crash.dump (unsigned int)(0x10080000 + 72*1024)
    append binary memory crash.dump 0x10080000 (0x10080000 + 72*1024)

    # Dump 64k of RAM starting at 0x20000000.
    #   First two words indicate memory range.
    append binary value crash.dump (unsigned int)0x20000000
    append binary value crash.dump (unsigned int)(0x20000000 + 64*1024)
    append binary memory crash.dump 0x20000000 (0x20000000 + 64*1024)

    # Dump the fault status registers as well.
    append binary value crash.dump (unsigned int)0xE000ED28
    append binary value crash.dump (unsigned int)(0xE000ED28 + 5*4)
    append binary memory crash.dump 0xE000ED28 (0xE000ED28 + 5*4)
end

document gcore
Generate core dump.

The generated core dump can be used with CrashDebug
(https://github.com/adamgreen/CrashDebug) to reload into GDB at a later point
in time or on another machine. The dump will be generated to a file named
"crash.dump".
end


# Some of the stock GDB commands have to be hooked to properly handle some of our custom commands.
# Enable fault catching when user requests execution to be resumed via continue command.
define hook-continue
    if ($catch_faults == 1)
        set var *(int*)0xE000EDFC |= 0x7F0
    end
end


# Command to dump the current amount of space allocated to the heap.
define heapsize
    set var $heap_base=(((unsigned int)&__HeapBase+7)&~7)
    set var $heap_end=(unsigned int)&_vStackTop - (unsigned int)4096
    printf "Used heap: %u bytes\n", (sbrk::currentHeapEnd - $heap_base)
    printf "Unused heap: %u bytes\n", ($heap_end - (unsigned int)sbrk::currentHeapEnd)
    printf "Total heap: %u bytes\n", ($heap_end - $heap_base)
end

document heapsize
Displays the current heap size.
end

# Command to dump the heap allocations (in-use and free).
define heapwalk
    set var $chunk_curr=(((unsigned int)&__HeapBase+7)&~7)
    set var $chunk_number=1
    set var $used_bytes=(unsigned int)0
    set var $free_bytes=(unsigned int)0
    if (sizeof(struct _reent) == 96)
        # newlib-nano library in use.
        set var $free_curr=(unsigned int)__malloc_free_list
        while ($chunk_curr < sbrk::currentHeapEnd)
            set var $chunk_size=*(unsigned int*)$chunk_curr
            set var $chunk_next=$chunk_curr + $chunk_size
            if ($chunk_curr == $free_curr)
                set var $chunk_free=1
                set var $free_curr=*(unsigned int*)($free_curr + 4)
            else
                set var $chunk_free=0
            end
            set var $chunk_orig=$chunk_curr + 4
            set var $chunk_curr=($chunk_orig + 7) & ~7
            set var $chunk_size=$chunk_size - 8
            printf "Chunk: %u  Address: 0x%08X  Size: %u  ", $chunk_number, $chunk_curr, $chunk_size
            if ($chunk_free)
                printf "FREE CHUNK"
                set var $free_bytes+=$chunk_size
            else
                set var $used_bytes+=$chunk_size
            end
            printf "\n"
            set var $chunk_curr=$chunk_next
            set var $chunk_number=$chunk_number+1
        end
    else
        # full newlib library in use.
        set var $heape= (unsigned int)sbrk::currentHeapEnd
        while ($chunk_curr < $heape)
            set var $chunk_size=*(unsigned int*)($chunk_curr + 4)
            set var $chunk_size&=~1
            set var $chunk_next=$chunk_curr + $chunk_size
            set var $chunk_inuse=(*(unsigned int*)($chunk_next + 4)) & 1

            # A 0-byte chunk at the beginning of the heap is the initial state before any allocs occur.
            if ($chunk_size == 0)
                loop_break
            end

            # The actual data starts past the 8 byte header.
            set var $chunk_orig=$chunk_curr + 8
            # The actual data is 4 bytes smaller than the total chunk since it can use the first word of the next chunk
            # as well since that is its footer.
            set var $chunk_size=$chunk_size - 4
            printf "Chunk: %u  Address: 0x%08X  Size: %u  ", $chunk_number, $chunk_orig, $chunk_size
            if ($chunk_inuse == 0)
                printf "FREE CHUNK"
                set var $free_bytes+=$chunk_size
            else
                set var $used_bytes+=$chunk_size
            end
            printf "\n"
            set var $chunk_curr=$chunk_next
            set var $chunk_number=$chunk_number+1
        end
    end
    printf "  Used bytes: %u\n", $used_bytes
    printf "  Free bytes: %u\n", $free_bytes
end

document heapwalk
Walks the heap and dumps each chunk encountered.
end




# Commands to run when GDB starts up to get it into a nice state for debugging embedded Cortex-M processors.
set target-charset ASCII
set print pretty on
set mem inaccessible-by-default off

# Default to enabling fault catching when user issues the continue execution command.
set var $catch_faults=1
