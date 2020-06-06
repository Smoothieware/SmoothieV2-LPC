#include "Pin.h"
#include "StringUtils.h"

#define NUM_GPIO_PORTS      8
#define NUM_GPIO_PINS       32
#define NUM_GPIO_NGROUPS    2

/*
 *
 * 20-bit Encoding:
 * 1111 1111 1100 0000 0000
 * 9876 5432 1098 7654 3210
 * ---- ---- ---- ---- ----
 * FFFU UDDI GWSS SSSP PPPP
 */

#define PINCONF_FUNC_SHIFT           (17)
#define PINCONF_FUNC_MASK            (7 << PINCONF_FUNC_SHIFT)

#define PINCONF_FUNC0                (0 << PINCONF_FUNC_SHIFT)
#define PINCONF_FUNC1                (1 << PINCONF_FUNC_SHIFT)
#define PINCONF_FUNC2                (2 << PINCONF_FUNC_SHIFT)
#define PINCONF_FUNC3                (3 << PINCONF_FUNC_SHIFT)
#define PINCONF_FUNC4                (4 << PINCONF_FUNC_SHIFT)
#define PINCONF_FUNC5                (5 << PINCONF_FUNC_SHIFT)
#define PINCONF_FUNC6                (6 << PINCONF_FUNC_SHIFT)
#define PINCONF_FUNC7                (7 << PINCONF_FUNC_SHIFT)

#define PINCONF_INBUFFER             (1 << 12)  /* Bit 11: 1=Enabled input buffer */
#define PINCONF_PULLUP               (1 << 16) /* Bit 16: 1=Pull-up */
#define PINCONF_PULLDOWN             (1 << 15) /* Bit 15: 1=Pull-down */
#define PINCONF_FLOAT                (0)       /* Bit 15-16=0 if neither */
#define PINCONF_GLITCH               (1 << 11)  /* Bit 10: 1=Glitch filter enable */
#define PINCONF_IS_PULLUP(p)          (((p) & PINCONF_PULLUP) != 0)
#define PINCONF_IS_PULLDOWN(p)        (((p) & PINCONF_PULLDOWN) != 0)
#define PINCONF_IS_FLOAT(p)           (((p) & (PINCONF_PULLUP|PINCONF_PULLDOWN) == 0)
#define PINCONF_INBUFFER_ENABLED(p)   (((p) & PINCONF_INBUFFER) != 0)
#define PINCONF_GLITCH_ENABLE(p)      (((p) & PINCONF_GLITCH) == 0)

#define PINCONF_DRIVE_SHIFT         (13)       /* Bits 12-13 = Pin drive strength */
#define PINCONF_DRIVE_MASK          (3 << PINCONF_DRIVE_SHIFT)
#define PINCONF_DRIVE_NORMAL        (0 << PINCONF_DRIVE_SHIFT)
#define PINCONF_DRIVE_MEDIUM        (1 << PINCONF_DRIVE_SHIFT)
#define PINCONF_DRIVE_HIGH          (2 << PINCONF_DRIVE_SHIFT)
#define PINCONF_DRIVE_ULTRA         (3 << PINCONF_DRIVE_SHIFT)

#define PINCONF_SLEW_FAST             (1 << 10)   /* Bit 9: 1=Alternate function */
#define PINCONF_SLEW_SLOW             (0)        /* Bit 9: 0=Normal function */
#define PINCONF_IS_SLEW_FAST(p)       (((p) & PINCONF_SLEW_FAST) != 0)
#define PINCONF_IS_SLOW_SLOW(p)       (((p) & PINCONF_SLEW_FAST) == 0)

#define PINCONF_PINS_SHIFT           (5)
#define PINCONF_PINS_MASK            (31 << PINCONF_PINS_SHIFT)
#define PINCONF_PINS0                (0 << PINCONF_PINS_SHIFT)
#define PINCONF_PINS1                (1 << PINCONF_PINS_SHIFT)
#define PINCONF_PINS2                (2 << PINCONF_PINS_SHIFT)
#define PINCONF_PINS3                (3 << PINCONF_PINS_SHIFT)
#define PINCONF_PINS4                (4 << PINCONF_PINS_SHIFT)
#define PINCONF_PINS5                (5 << PINCONF_PINS_SHIFT)
#define PINCONF_PINS6                (6 << PINCONF_PINS_SHIFT)
#define PINCONF_PINS7                (7 << PINCONF_PINS_SHIFT)
#define PINCONF_PINS8                (8 << PINCONF_PINS_SHIFT)
#define PINCONF_PINS9                (9 << PINCONF_PINS_SHIFT)
#define PINCONF_PINSA                (10 << PINCONF_PINS_SHIFT)
#define PINCONF_PINSB                (11 << PINCONF_PINS_SHIFT)
#define PINCONF_PINSC                (12 << PINCONF_PINS_SHIFT)
#define PINCONF_PINSD                (13 << PINCONF_PINS_SHIFT)
#define PINCONF_PINSE                (14 << PINCONF_PINS_SHIFT)
#define PINCONF_PINSF                (15 << PINCONF_PINS_SHIFT)
#define PINCONF_PINSG                (16 << PINCONF_PINS_SHIFT)
#define PINCONF_PINSH                (17 << PINCONF_PINS_SHIFT)
#define PINCONF_PINSI                (18 << PINCONF_PINS_SHIFT)
#define PINCONF_PINSJ                (19 << PINCONF_PINS_SHIFT)
#define PINCONF_PINSK                (20 << PINCONF_PINS_SHIFT)
#define PINCONF_PINSL                (21 << PINCONF_PINS_SHIFT)
#define PINCONF_PINSM                (22 << PINCONF_PINS_SHIFT)
#define PINCONF_PINSN                (23 << PINCONF_PINS_SHIFT)
#define PINCONF_PINSO                (24 << PINCONF_PINS_SHIFT)
#define PINCONF_PINSP                (25 << PINCONF_PINS_SHIFT)
#define PINCONF_PINSQ                (26 << PINCONF_PINS_SHIFT)
#define PINCONF_PINSR                (27 << PINCONF_PINS_SHIFT)
#define PINCONF_PINSS                (28 << PINCONF_PINS_SHIFT)
#define PINCONF_PINST                (29 << PINCONF_PINS_SHIFT)
#define PINCONF_PINSU                (30 << PINCONF_PINS_SHIFT)
#define PINCONF_PINSV                (31 << PINCONF_PINS_SHIFT)

#define PINCONF_PIN_SHIFT            (0)
#define PINCONF_PIN_MASK             (31 << PINCONF_PIN_SHIFT)
#define PINCONF_PIN_0                (0 << PINCONF_PIN_SHIFT)
#define PINCONF_PIN_1                (1 << PINCONF_PIN_SHIFT)
#define PINCONF_PIN_2                (2 << PINCONF_PIN_SHIFT)
#define PINCONF_PIN_3                (3 << PINCONF_PIN_SHIFT)
#define PINCONF_PIN_4                (4 << PINCONF_PIN_SHIFT)
#define PINCONF_PIN_5                (5 << PINCONF_PIN_SHIFT)
#define PINCONF_PIN_6                (6 << PINCONF_PIN_SHIFT)
#define PINCONF_PIN_7                (7 << PINCONF_PIN_SHIFT)
#define PINCONF_PIN_8                (8 << PINCONF_PIN_SHIFT)
#define PINCONF_PIN_9                (9 << PINCONF_PIN_SHIFT)
#define PINCONF_PIN_10               (10 << PINCONF_PIN_SHIFT)
#define PINCONF_PIN_11               (11 << PINCONF_PIN_SHIFT)
#define PINCONF_PIN_12               (12 << PINCONF_PIN_SHIFT)
#define PINCONF_PIN_13               (13 << PINCONF_PIN_SHIFT)
#define PINCONF_PIN_14               (14 << PINCONF_PIN_SHIFT)
#define PINCONF_PIN_15               (15 << PINCONF_PIN_SHIFT)
#define PINCONF_PIN_16               (16 << PINCONF_PIN_SHIFT)
#define PINCONF_PIN_17               (17 << PINCONF_PIN_SHIFT)
#define PINCONF_PIN_18               (18 << PINCONF_PIN_SHIFT)
#define PINCONF_PIN_19               (19 << PINCONF_PIN_SHIFT)
#define PINCONF_PIN_20               (20 << PINCONF_PIN_SHIFT)
#define PINCONF_PIN_21               (21 << PINCONF_PIN_SHIFT)
#define PINCONF_PIN_22               (22 << PINCONF_PIN_SHIFT)
#define PINCONF_PIN_23               (23 << PINCONF_PIN_SHIFT)
#define PINCONF_PIN_24               (24 << PINCONF_PIN_SHIFT)
#define PINCONF_PIN_25               (25 << PINCONF_PIN_SHIFT)
#define PINCONF_PIN_26               (26 << PINCONF_PIN_SHIFT)
#define PINCONF_PIN_27               (27 << PINCONF_PIN_SHIFT)
#define PINCONF_PIN_28               (28 << PINCONF_PIN_SHIFT)
#define PINCONF_PIN_29               (29 << PINCONF_PIN_SHIFT)
#define PINCONF_PIN_30               (30 << PINCONF_PIN_SHIFT)
#define PINCONF_PIN_31               (31 << PINCONF_PIN_SHIFT)

#define PINCONF_GPIO0p0           (PINCONF_FUNC0|PINCONF_INBUFFER|PINCONF_PINS0|PINCONF_PIN_0)
#define PINCONF_GPIO0p1           (PINCONF_FUNC0|PINCONF_INBUFFER|PINCONF_PINS0|PINCONF_PIN_1)
#define PINCONF_GPIO0p2           (PINCONF_FUNC0|PINCONF_INBUFFER|PINCONF_PINS1|PINCONF_PIN_15)
#define PINCONF_GPIO0p3           (PINCONF_FUNC0|PINCONF_INBUFFER|PINCONF_PINS1|PINCONF_PIN_16)
#define PINCONF_GPIO0p4           (PINCONF_FUNC0|PINCONF_INBUFFER|PINCONF_PINS1|PINCONF_PIN_0)
#define PINCONF_GPIO0p5           (PINCONF_FUNC0|PINCONF_INBUFFER|PINCONF_PINS6|PINCONF_PIN_6)
#define PINCONF_GPIO0p6           (PINCONF_FUNC0|PINCONF_INBUFFER|PINCONF_PINS3|PINCONF_PIN_6)
#define PINCONF_GPIO0p7           (PINCONF_FUNC0|PINCONF_INBUFFER|PINCONF_PINS2|PINCONF_PIN_7)
#define PINCONF_GPIO0p8           (PINCONF_FUNC0|PINCONF_INBUFFER|PINCONF_PINS1|PINCONF_PIN_1)
#define PINCONF_GPIO0p9           (PINCONF_FUNC0|PINCONF_INBUFFER|PINCONF_PINS1|PINCONF_PIN_2)
#define PINCONF_GPIO0p10          (PINCONF_FUNC0|PINCONF_INBUFFER|PINCONF_PINS1|PINCONF_PIN_3)
#define PINCONF_GPIO0p11          (PINCONF_FUNC0|PINCONF_INBUFFER|PINCONF_PINS1|PINCONF_PIN_4)
#define PINCONF_GPIO0p12          (PINCONF_FUNC0|PINCONF_INBUFFER|PINCONF_PINS1|PINCONF_PIN_17)
#define PINCONF_GPIO0p13          (PINCONF_FUNC0|PINCONF_INBUFFER|PINCONF_PINS1|PINCONF_PIN_18)
#define PINCONF_GPIO0p14          (PINCONF_FUNC0|PINCONF_INBUFFER|PINCONF_PINS2|PINCONF_PIN_10)
#define PINCONF_GPIO0p15          (PINCONF_FUNC0|PINCONF_INBUFFER|PINCONF_PINS1|PINCONF_PIN_20)
#define PINCONF_GPIO1p0           (PINCONF_FUNC0|PINCONF_INBUFFER|PINCONF_PINS1|PINCONF_PIN_7)
#define PINCONF_GPIO1p1           (PINCONF_FUNC0|PINCONF_INBUFFER|PINCONF_PINS1|PINCONF_PIN_8)
#define PINCONF_GPIO1p2           (PINCONF_FUNC0|PINCONF_INBUFFER|PINCONF_PINS1|PINCONF_PIN_9)
#define PINCONF_GPIO1p3           (PINCONF_FUNC0|PINCONF_INBUFFER|PINCONF_PINS1|PINCONF_PIN_10)
#define PINCONF_GPIO1p4           (PINCONF_FUNC0|PINCONF_INBUFFER|PINCONF_PINS1|PINCONF_PIN_11)
#define PINCONF_GPIO1p5           (PINCONF_FUNC0|PINCONF_INBUFFER|PINCONF_PINS1|PINCONF_PIN_12)
#define PINCONF_GPIO1p6           (PINCONF_FUNC0|PINCONF_INBUFFER|PINCONF_PINS1|PINCONF_PIN_13)
#define PINCONF_GPIO1p7           (PINCONF_FUNC0|PINCONF_INBUFFER|PINCONF_PINS1|PINCONF_PIN_14)
#define PINCONF_GPIO1p8           (PINCONF_FUNC0|PINCONF_INBUFFER|PINCONF_PINS1|PINCONF_PIN_5)
#define PINCONF_GPIO1p9           (PINCONF_FUNC0|PINCONF_INBUFFER|PINCONF_PINS1|PINCONF_PIN_6)
#define PINCONF_GPIO1p10          (PINCONF_FUNC0|PINCONF_INBUFFER|PINCONF_PINS2|PINCONF_PIN_9)
#define PINCONF_GPIO1p11          (PINCONF_FUNC0|PINCONF_INBUFFER|PINCONF_PINS2|PINCONF_PIN_11)
#define PINCONF_GPIO1p12          (PINCONF_FUNC0|PINCONF_INBUFFER|PINCONF_PINS2|PINCONF_PIN_12)
#define PINCONF_GPIO1p13          (PINCONF_FUNC0|PINCONF_INBUFFER|PINCONF_PINS2|PINCONF_PIN_13)
#define PINCONF_GPIO1p14          (PINCONF_FUNC0|PINCONF_INBUFFER|PINCONF_PINS3|PINCONF_PIN_4)
#define PINCONF_GPIO1p15          (PINCONF_FUNC0|PINCONF_INBUFFER|PINCONF_PINS3|PINCONF_PIN_5)
#define PINCONF_GPIO2p0           (PINCONF_FUNC0|PINCONF_INBUFFER|PINCONF_PINS4|PINCONF_PIN_0)
#define PINCONF_GPIO2p1           (PINCONF_FUNC0|PINCONF_INBUFFER|PINCONF_PINS4|PINCONF_PIN_1)
#define PINCONF_GPIO2p2           (PINCONF_FUNC0|PINCONF_INBUFFER|PINCONF_PINS4|PINCONF_PIN_2)
#define PINCONF_GPIO2p3           (PINCONF_FUNC0|PINCONF_INBUFFER|PINCONF_PINS4|PINCONF_PIN_3)
#define PINCONF_GPIO2p4           (PINCONF_FUNC0|PINCONF_INBUFFER|PINCONF_PINS4|PINCONF_PIN_4)
#define PINCONF_GPIO2p5           (PINCONF_FUNC0|PINCONF_INBUFFER|PINCONF_PINS4|PINCONF_PIN_5)
#define PINCONF_GPIO2p6           (PINCONF_FUNC0|PINCONF_INBUFFER|PINCONF_PINS4|PINCONF_PIN_6)
#define PINCONF_GPIO2p7           (PINCONF_FUNC0|PINCONF_INBUFFER|PINCONF_PINS5|PINCONF_PIN_7)
#define PINCONF_GPIO2p8           (PINCONF_FUNC0|PINCONF_INBUFFER|PINCONF_PINS6|PINCONF_PIN_12)
#define PINCONF_GPIO2p9           (PINCONF_FUNC0|PINCONF_INBUFFER|PINCONF_PINS5|PINCONF_PIN_0)
#define PINCONF_GPIO2p10          (PINCONF_FUNC0|PINCONF_INBUFFER|PINCONF_PINS5|PINCONF_PIN_1)
#define PINCONF_GPIO2p11          (PINCONF_FUNC0|PINCONF_INBUFFER|PINCONF_PINS5|PINCONF_PIN_2)
#define PINCONF_GPIO2p12          (PINCONF_FUNC0|PINCONF_INBUFFER|PINCONF_PINS5|PINCONF_PIN_3)
#define PINCONF_GPIO2p13          (PINCONF_FUNC0|PINCONF_INBUFFER|PINCONF_PINS5|PINCONF_PIN_4)
#define PINCONF_GPIO2p14          (PINCONF_FUNC0|PINCONF_INBUFFER|PINCONF_PINS5|PINCONF_PIN_5)
#define PINCONF_GPIO2p15          (PINCONF_FUNC0|PINCONF_INBUFFER|PINCONF_PINS5|PINCONF_PIN_6)
#define PINCONF_GPIO3p0           (PINCONF_FUNC0|PINCONF_INBUFFER|PINCONF_PINS6|PINCONF_PIN_1)
#define PINCONF_GPIO3p1           (PINCONF_FUNC0|PINCONF_INBUFFER|PINCONF_PINS6|PINCONF_PIN_2)
#define PINCONF_GPIO3p2           (PINCONF_FUNC0|PINCONF_INBUFFER|PINCONF_PINS6|PINCONF_PIN_3)
#define PINCONF_GPIO3p3           (PINCONF_FUNC0|PINCONF_INBUFFER|PINCONF_PINS6|PINCONF_PIN_4)
#define PINCONF_GPIO3p4           (PINCONF_FUNC0|PINCONF_INBUFFER|PINCONF_PINS6|PINCONF_PIN_5)
#define PINCONF_GPIO3p5           (PINCONF_FUNC0|PINCONF_INBUFFER|PINCONF_PINS6|PINCONF_PIN_9)
#define PINCONF_GPIO3p6           (PINCONF_FUNC0|PINCONF_INBUFFER|PINCONF_PINS6|PINCONF_PIN_10)
#define PINCONF_GPIO3p7           (PINCONF_FUNC0|PINCONF_INBUFFER|PINCONF_PINS6|PINCONF_PIN_11)
#define PINCONF_GPIO3p8           (PINCONF_FUNC0|PINCONF_INBUFFER|PINCONF_PINS7|PINCONF_PIN_0)
#define PINCONF_GPIO3p9           (PINCONF_FUNC0|PINCONF_INBUFFER|PINCONF_PINS7|PINCONF_PIN_1)
#define PINCONF_GPIO3p10          (PINCONF_FUNC0|PINCONF_INBUFFER|PINCONF_PINS7|PINCONF_PIN_2)
#define PINCONF_GPIO3p11          (PINCONF_FUNC0|PINCONF_INBUFFER|PINCONF_PINS7|PINCONF_PIN_3)
#define PINCONF_GPIO3p12          (PINCONF_FUNC0|PINCONF_INBUFFER|PINCONF_PINS7|PINCONF_PIN_4)
#define PINCONF_GPIO3p13          (PINCONF_FUNC0|PINCONF_INBUFFER|PINCONF_PINS7|PINCONF_PIN_5)
#define PINCONF_GPIO3p14          (PINCONF_FUNC0|PINCONF_INBUFFER|PINCONF_PINS7|PINCONF_PIN_6)
#define PINCONF_GPIO3p15          (PINCONF_FUNC0|PINCONF_INBUFFER|PINCONF_PINS7|PINCONF_PIN_7)
#define PINCONF_GPIO4p0           (PINCONF_FUNC0|PINCONF_INBUFFER|PINCONF_PINS8|PINCONF_PIN_0)
#define PINCONF_GPIO4p1           (PINCONF_FUNC0|PINCONF_INBUFFER|PINCONF_PINS8|PINCONF_PIN_1)
#define PINCONF_GPIO4p2           (PINCONF_FUNC0|PINCONF_INBUFFER|PINCONF_PINS8|PINCONF_PIN_2)
#define PINCONF_GPIO4p3           (PINCONF_FUNC0|PINCONF_INBUFFER|PINCONF_PINS8|PINCONF_PIN_3)
#define PINCONF_GPIO4p4           (PINCONF_FUNC0|PINCONF_INBUFFER|PINCONF_PINS8|PINCONF_PIN_4)
#define PINCONF_GPIO4p5           (PINCONF_FUNC0|PINCONF_INBUFFER|PINCONF_PINS8|PINCONF_PIN_5)
#define PINCONF_GPIO4p6           (PINCONF_FUNC0|PINCONF_INBUFFER|PINCONF_PINS8|PINCONF_PIN_6)
#define PINCONF_GPIO4p7           (PINCONF_FUNC0|PINCONF_INBUFFER|PINCONF_PINS8|PINCONF_PIN_7)
#define PINCONF_GPIO4p8           (PINCONF_FUNC0|PINCONF_PINSA|PINCONF_PIN_1)
#define PINCONF_GPIO4p9           (PINCONF_FUNC0|PINCONF_INBUFFER|PINCONF_PINSA|PINCONF_PIN_2)
#define PINCONF_GPIO4p10          (PINCONF_FUNC0|PINCONF_INBUFFER|PINCONF_PINSA|PINCONF_PIN_3)
#define PINCONF_GPIO4p11          (PINCONF_FUNC0|PINCONF_INBUFFER|PINCONF_PINS9|PINCONF_PIN_6)
#define PINCONF_GPIO4p12          (PINCONF_FUNC0|PINCONF_INBUFFER|PINCONF_PINS9|PINCONF_PIN_0)
#define PINCONF_GPIO4p13          (PINCONF_FUNC0|PINCONF_INBUFFER|PINCONF_PINS9|PINCONF_PIN_1)
#define PINCONF_GPIO4p14          (PINCONF_FUNC0|PINCONF_INBUFFER|PINCONF_PINS9|PINCONF_PIN_2)
#define PINCONF_GPIO4p15          (PINCONF_FUNC0|PINCONF_INBUFFER|PINCONF_PINS9|PINCONF_PIN_3)
#define PINCONF_GPIO5p0           (PINCONF_FUNC4|PINCONF_INBUFFER|PINCONF_PINS2|PINCONF_PIN_0)
#define PINCONF_GPIO5p1           (PINCONF_FUNC4|PINCONF_INBUFFER|PINCONF_PINS2|PINCONF_PIN_1)
#define PINCONF_GPIO5p2           (PINCONF_FUNC4|PINCONF_INBUFFER|PINCONF_PINS2|PINCONF_PIN_2)
#define PINCONF_GPIO5p3           (PINCONF_FUNC4|PINCONF_INBUFFER|PINCONF_PINS2|PINCONF_PIN_3)
#define PINCONF_GPIO5p4           (PINCONF_FUNC4|PINCONF_INBUFFER|PINCONF_PINS2|PINCONF_PIN_4)
#define PINCONF_GPIO5p5           (PINCONF_FUNC4|PINCONF_INBUFFER|PINCONF_PINS2|PINCONF_PIN_5)
#define PINCONF_GPIO5p6           (PINCONF_FUNC4|PINCONF_INBUFFER|PINCONF_PINS2|PINCONF_PIN_6)
#define PINCONF_GPIO5p7           (PINCONF_FUNC4|PINCONF_INBUFFER|PINCONF_PINS2|PINCONF_PIN_8)
#define PINCONF_GPIO5p8           (PINCONF_FUNC4|PINCONF_INBUFFER|PINCONF_PINS3|PINCONF_PIN_1)
#define PINCONF_GPIO5p9           (PINCONF_FUNC4|PINCONF_INBUFFER|PINCONF_PINS3|PINCONF_PIN_2)
#define PINCONF_GPIO5p10          (PINCONF_FUNC4|PINCONF_INBUFFER|PINCONF_PINS3|PINCONF_PIN_7)
#define PINCONF_GPIO5p11          (PINCONF_FUNC4|PINCONF_INBUFFER|PINCONF_PINS3|PINCONF_PIN_8)
#define PINCONF_GPIO5p12          (PINCONF_FUNC4|PINCONF_INBUFFER|PINCONF_PINS4|PINCONF_PIN_8)
#define PINCONF_GPIO5p13          (PINCONF_FUNC4|PINCONF_INBUFFER|PINCONF_PINS4|PINCONF_PIN_9)
#define PINCONF_GPIO5p14          (PINCONF_FUNC4|PINCONF_INBUFFER|PINCONF_PINS4|PINCONF_PIN_10)
#define PINCONF_GPIO5p15          (PINCONF_FUNC4|PINCONF_INBUFFER|PINCONF_PINS6|PINCONF_PIN_7)
#define PINCONF_GPIO5p16          (PINCONF_FUNC4|PINCONF_INBUFFER|PINCONF_PINS6|PINCONF_PIN_8)
#define PINCONF_GPIO5p17          (PINCONF_FUNC4|PINCONF_INBUFFER|PINCONF_PINS9|PINCONF_PIN_4)
#define PINCONF_GPIO5p18          (PINCONF_FUNC4|PINCONF_INBUFFER|PINCONF_PINS9|PINCONF_PIN_5)
#define PINCONF_GPIO5p19          (PINCONF_FUNC4|PINCONF_INBUFFER|PINCONF_PINSA|PINCONF_PIN_4)
#define PINCONF_GPIO5p20          (PINCONF_FUNC4|PINCONF_INBUFFER|PINCONF_PINSB|PINCONF_PIN_0)
#define PINCONF_GPIO5p21          (PINCONF_FUNC4|PINCONF_INBUFFER|PINCONF_PINSB|PINCONF_PIN_1)
#define PINCONF_GPIO5p22          (PINCONF_FUNC4|PINCONF_INBUFFER|PINCONF_PINSB|PINCONF_PIN_2)
#define PINCONF_GPIO5p23          (PINCONF_FUNC4|PINCONF_INBUFFER|PINCONF_PINSB|PINCONF_PIN_3)
#define PINCONF_GPIO5p24          (PINCONF_FUNC4|PINCONF_INBUFFER|PINCONF_PINSB|PINCONF_PIN_4)
#define PINCONF_GPIO5p25          (PINCONF_FUNC4|PINCONF_INBUFFER|PINCONF_PINSB|PINCONF_PIN_5)
#define PINCONF_GPIO5p26          (PINCONF_FUNC4|PINCONF_INBUFFER|PINCONF_PINSB|PINCONF_PIN_6)
#define PINCONF_GPIO6p0           (PINCONF_FUNC4|PINCONF_INBUFFER|PINCONF_PINSC|PINCONF_PIN_1)
#define PINCONF_GPIO6p1           (PINCONF_FUNC4|PINCONF_INBUFFER|PINCONF_PINSC|PINCONF_PIN_2)
#define PINCONF_GPIO6p2           (PINCONF_FUNC4|PINCONF_INBUFFER|PINCONF_PINSC|PINCONF_PIN_3)
#define PINCONF_GPIO6p3           (PINCONF_FUNC4|PINCONF_INBUFFER|PINCONF_PINSC|PINCONF_PIN_4)
#define PINCONF_GPIO6p4           (PINCONF_FUNC4|PINCONF_INBUFFER|PINCONF_PINSC|PINCONF_PIN_5)
#define PINCONF_GPIO6p5           (PINCONF_FUNC4|PINCONF_INBUFFER|PINCONF_PINSC|PINCONF_PIN_6)
#define PINCONF_GPIO6p6           (PINCONF_FUNC4|PINCONF_INBUFFER|PINCONF_PINSC|PINCONF_PIN_7)
#define PINCONF_GPIO6p7           (PINCONF_FUNC4|PINCONF_INBUFFER|PINCONF_PINSC|PINCONF_PIN_8)
#define PINCONF_GPIO6p8           (PINCONF_FUNC4|PINCONF_INBUFFER|PINCONF_PINSC|PINCONF_PIN_9)
#define PINCONF_GPIO6p9           (PINCONF_FUNC4|PINCONF_INBUFFER|PINCONF_PINSC|PINCONF_PIN_10)
#define PINCONF_GPIO6p10          (PINCONF_FUNC4|PINCONF_INBUFFER|PINCONF_PINSC|PINCONF_PIN_11)
#define PINCONF_GPIO6p11          (PINCONF_FUNC4|PINCONF_INBUFFER|PINCONF_PINSC|PINCONF_PIN_12)
#define PINCONF_GPIO6p12          (PINCONF_FUNC4|PINCONF_INBUFFER|PINCONF_PINSC|PINCONF_PIN_13)
#define PINCONF_GPIO6p13          (PINCONF_FUNC4|PINCONF_INBUFFER|PINCONF_PINSC|PINCONF_PIN_14)
#define PINCONF_GPIO6p14          (PINCONF_FUNC4|PINCONF_INBUFFER|PINCONF_PINSD|PINCONF_PIN_0)
#define PINCONF_GPIO6p15          (PINCONF_FUNC4|PINCONF_INBUFFER|PINCONF_PINSD|PINCONF_PIN_1)
#define PINCONF_GPIO6p16          (PINCONF_FUNC4|PINCONF_INBUFFER|PINCONF_PINSD|PINCONF_PIN_2)
#define PINCONF_GPIO6p17          (PINCONF_FUNC4|PINCONF_INBUFFER|PINCONF_PINSD|PINCONF_PIN_3)
#define PINCONF_GPIO6p18          (PINCONF_FUNC4|PINCONF_INBUFFER|PINCONF_PINSD|PINCONF_PIN_4)
#define PINCONF_GPIO6p19          (PINCONF_FUNC4|PINCONF_INBUFFER|PINCONF_PINSD|PINCONF_PIN_5)
#define PINCONF_GPIO6p20          (PINCONF_FUNC4|PINCONF_INBUFFER|PINCONF_PINSD|PINCONF_PIN_6)
#define PINCONF_GPIO6p21          (PINCONF_FUNC4|PINCONF_INBUFFER|PINCONF_PINSD|PINCONF_PIN_7)
#define PINCONF_GPIO6p22          (PINCONF_FUNC4|PINCONF_INBUFFER|PINCONF_PINSD|PINCONF_PIN_8)
#define PINCONF_GPIO6p23          (PINCONF_FUNC4|PINCONF_INBUFFER|PINCONF_PINSD|PINCONF_PIN_9)
#define PINCONF_GPIO6p24          (PINCONF_FUNC4|PINCONF_INBUFFER|PINCONF_PINSD|PINCONF_PIN_10)
#define PINCONF_GPIO6p25          (PINCONF_FUNC4|PINCONF_INBUFFER|PINCONF_PINSD|PINCONF_PIN_11)
#define PINCONF_GPIO6p26          (PINCONF_FUNC4|PINCONF_INBUFFER|PINCONF_PINSD|PINCONF_PIN_12)
#define PINCONF_GPIO6p27          (PINCONF_FUNC4|PINCONF_INBUFFER|PINCONF_PINSD|PINCONF_PIN_13)
#define PINCONF_GPIO6p28          (PINCONF_FUNC4|PINCONF_INBUFFER|PINCONF_PINSD|PINCONF_PIN_14)
#define PINCONF_GPIO6p29          (PINCONF_FUNC4|PINCONF_INBUFFER|PINCONF_PINSD|PINCONF_PIN_15)
#define PINCONF_GPIO6p30          (PINCONF_FUNC4|PINCONF_INBUFFER|PINCONF_PINSD|PINCONF_PIN_16)
#define PINCONF_GPIO7p0           (PINCONF_FUNC4|PINCONF_INBUFFER|PINCONF_PINSE|PINCONF_PIN_0)
#define PINCONF_GPIO7p1           (PINCONF_FUNC4|PINCONF_INBUFFER|PINCONF_PINSE|PINCONF_PIN_1)
#define PINCONF_GPIO7p2           (PINCONF_FUNC4|PINCONF_INBUFFER|PINCONF_PINSE|PINCONF_PIN_2)
#define PINCONF_GPIO7p3           (PINCONF_FUNC4|PINCONF_INBUFFER|PINCONF_PINSE|PINCONF_PIN_3)
#define PINCONF_GPIO7p4           (PINCONF_FUNC4|PINCONF_INBUFFER|PINCONF_PINSE|PINCONF_PIN_4)
#define PINCONF_GPIO7p5           (PINCONF_FUNC4|PINCONF_INBUFFER|PINCONF_PINSE|PINCONF_PIN_5)
#define PINCONF_GPIO7p6           (PINCONF_FUNC4|PINCONF_INBUFFER|PINCONF_PINSE|PINCONF_PIN_6)
#define PINCONF_GPIO7p7           (PINCONF_FUNC4|PINCONF_INBUFFER|PINCONF_PINSE|PINCONF_PIN_7)
#define PINCONF_GPIO7p8           (PINCONF_FUNC4|PINCONF_INBUFFER|PINCONF_PINSE|PINCONF_PIN_8)
#define PINCONF_GPIO7p9           (PINCONF_FUNC4|PINCONF_INBUFFER|PINCONF_PINSE|PINCONF_PIN_9)
#define PINCONF_GPIO7p10          (PINCONF_FUNC4|PINCONF_INBUFFER|PINCONF_PINSE|PINCONF_PIN_10)
#define PINCONF_GPIO7p11          (PINCONF_FUNC4|PINCONF_INBUFFER|PINCONF_PINSE|PINCONF_PIN_11)
#define PINCONF_GPIO7p12          (PINCONF_FUNC4|PINCONF_INBUFFER|PINCONF_PINSE|PINCONF_PIN_12)
#define PINCONF_GPIO7p13          (PINCONF_FUNC4|PINCONF_INBUFFER|PINCONF_PINSE|PINCONF_PIN_13)
#define PINCONF_GPIO7p14          (PINCONF_FUNC4|PINCONF_INBUFFER|PINCONF_PINSE|PINCONF_PIN_14)
#define PINCONF_GPIO7p15          (PINCONF_FUNC4|PINCONF_INBUFFER|PINCONF_PINSE|PINCONF_PIN_15)
#define PINCONF_GPIO7p16          (PINCONF_FUNC4|PINCONF_INBUFFER|PINCONF_PINSF|PINCONF_PIN_1)
#define PINCONF_GPIO7p17          (PINCONF_FUNC4|PINCONF_INBUFFER|PINCONF_PINSF|PINCONF_PIN_2)
#define PINCONF_GPIO7p18          (PINCONF_FUNC4|PINCONF_INBUFFER|PINCONF_PINSF|PINCONF_PIN_3)
#define PINCONF_GPIO7p19          (PINCONF_FUNC4|PINCONF_INBUFFER|PINCONF_PINSF|PINCONF_PIN_5)
#define PINCONF_GPIO7p20          (PINCONF_FUNC4|PINCONF_INBUFFER|PINCONF_PINSF|PINCONF_PIN_6)
#define PINCONF_GPIO7p21          (PINCONF_FUNC4|PINCONF_INBUFFER|PINCONF_PINSF|PINCONF_PIN_7)
#define PINCONF_GPIO7p22          (PINCONF_FUNC4|PINCONF_INBUFFER|PINCONF_PINSF|PINCONF_PIN_8)
#define PINCONF_GPIO7p23          (PINCONF_FUNC4|PINCONF_INBUFFER|PINCONF_PINSF|PINCONF_PIN_9)
#define PINCONF_GPIO7p24          (PINCONF_FUNC4|PINCONF_INBUFFER|PINCONF_PINSF|PINCONF_PIN_10)
#define PINCONF_GPIO7p25          (PINCONF_FUNC4|PINCONF_INBUFFER|PINCONF_PINSF|PINCONF_PIN_11)

Pin::Pin()
{
    this->inverting = false;
    this->open_drain = false;
    this->valid = false;
}

Pin::Pin(const char *s)
{
    this->inverting = false;
    this->open_drain = false;
    this->valid = false;
    from_string(s);
}

Pin::Pin(const char *s, TYPE_T t)
{
    this->inverting = false;
    this->valid = false;
    this->open_drain = false;
    if(from_string(s) != nullptr) {
        switch(t) {
            case AS_INPUT: as_input(); break;
            case AS_OUTPUT: as_output(); break;
        }
    }
}

Pin::~Pin()
{
    // TODO trouble is we copy pins so this would deallocate a used pin, see Robot actuator pins
    // deallocate it in the bitset, but leaves th ephysical port as it was
    // if(valid) {
    //     uint16_t port = gpiocfg >> GPIO_PORT_SHIFT;
    //     uint16_t pin = gpiocfg & GPIO_PIN_MASK;
    //     set_allocated(port, pin, false);
    // }
}

// bitset to indicate a port/pin has been configured
#include <bitset>
static std::bitset<256> allocated_pins;
bool Pin::set_allocated(uint8_t port, uint8_t pin, bool set)
{
    uint8_t n = (port * NUM_GPIO_PINS) + pin;

    if(!set) {
        // deallocate it
        allocated_pins.reset(n);
        return true;
    }

    if(!allocated_pins[n]) {
        // if not set yet then set it
        allocated_pins.set(n);
        return true;
    }

    // indicate it was already set
    return false;
}

// look up table to convert GPIO port/pin into a PINCONF
static const uint32_t port_pin_lut[NUM_GPIO_PORTS][NUM_GPIO_PINS] = {
    {
        PINCONF_GPIO0p0, PINCONF_GPIO0p1, PINCONF_GPIO0p2,  PINCONF_GPIO0p3,  PINCONF_GPIO0p4,  PINCONF_GPIO0p5,  PINCONF_GPIO0p6, PINCONF_GPIO0p7,
        PINCONF_GPIO0p8, PINCONF_GPIO0p9, PINCONF_GPIO0p10, PINCONF_GPIO0p11, PINCONF_GPIO0p12, PINCONF_GPIO0p13, PINCONF_GPIO0p14, PINCONF_GPIO0p15,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    }, //port 0

    {
        PINCONF_GPIO1p0,  PINCONF_GPIO1p1,  PINCONF_GPIO1p2,  PINCONF_GPIO1p3,  PINCONF_GPIO1p4,  PINCONF_GPIO1p5,  PINCONF_GPIO1p6,  PINCONF_GPIO1p7,
        PINCONF_GPIO1p8,  PINCONF_GPIO1p9,  PINCONF_GPIO1p10, PINCONF_GPIO1p11, PINCONF_GPIO1p12, PINCONF_GPIO1p13, PINCONF_GPIO1p14, PINCONF_GPIO1p15,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    }, //port 1

    {
        PINCONF_GPIO2p0,  PINCONF_GPIO2p1,  PINCONF_GPIO2p2,  PINCONF_GPIO2p3,  PINCONF_GPIO2p4,  PINCONF_GPIO2p5,  PINCONF_GPIO2p6,  PINCONF_GPIO2p7,
        PINCONF_GPIO2p8,  PINCONF_GPIO2p9,  PINCONF_GPIO2p10, PINCONF_GPIO2p11, PINCONF_GPIO2p12, PINCONF_GPIO2p13, PINCONF_GPIO2p14, PINCONF_GPIO2p15,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    }, //port 2

    {
        PINCONF_GPIO3p0,  PINCONF_GPIO3p1,  PINCONF_GPIO3p2,  PINCONF_GPIO3p3,  PINCONF_GPIO3p4,  PINCONF_GPIO3p5,  PINCONF_GPIO3p6,  PINCONF_GPIO3p7,
        PINCONF_GPIO3p8,  PINCONF_GPIO3p9,  PINCONF_GPIO3p10, PINCONF_GPIO3p11, PINCONF_GPIO3p12, PINCONF_GPIO3p13, PINCONF_GPIO3p14, PINCONF_GPIO3p15,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    }, //port 3

    {
        PINCONF_GPIO4p0,  PINCONF_GPIO4p1,  PINCONF_GPIO4p2,  PINCONF_GPIO4p3,  PINCONF_GPIO4p4,  PINCONF_GPIO4p5,  PINCONF_GPIO4p6,  PINCONF_GPIO4p7,
        PINCONF_GPIO4p8,  PINCONF_GPIO4p9,  PINCONF_GPIO4p10, PINCONF_GPIO4p11, PINCONF_GPIO4p12, PINCONF_GPIO4p13, PINCONF_GPIO4p14, PINCONF_GPIO4p15,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    }, //port 4

    {
        PINCONF_GPIO5p0,  PINCONF_GPIO5p1,  PINCONF_GPIO5p2,  PINCONF_GPIO5p3,  PINCONF_GPIO5p4,  PINCONF_GPIO5p5,  PINCONF_GPIO5p6,  PINCONF_GPIO5p7,
        PINCONF_GPIO5p8,  PINCONF_GPIO5p9,  PINCONF_GPIO5p10, PINCONF_GPIO5p11, PINCONF_GPIO5p12, PINCONF_GPIO5p13, PINCONF_GPIO5p14, PINCONF_GPIO5p15,
        PINCONF_GPIO5p16, PINCONF_GPIO5p17, PINCONF_GPIO5p18, PINCONF_GPIO5p19, PINCONF_GPIO5p20, PINCONF_GPIO5p21, PINCONF_GPIO5p22, PINCONF_GPIO5p23,
        PINCONF_GPIO5p24, PINCONF_GPIO5p25, PINCONF_GPIO5p26,
        0, 0, 0, 0, 0
    }, //port 5

    {
        PINCONF_GPIO6p0, PINCONF_GPIO6p1,  PINCONF_GPIO6p2,  PINCONF_GPIO6p3,  PINCONF_GPIO6p4,  PINCONF_GPIO6p5,  PINCONF_GPIO6p6,  PINCONF_GPIO6p7,
        PINCONF_GPIO6p8,  PINCONF_GPIO6p9,  PINCONF_GPIO6p10, PINCONF_GPIO6p11, PINCONF_GPIO6p12, PINCONF_GPIO6p13, PINCONF_GPIO6p14, PINCONF_GPIO6p15,
        PINCONF_GPIO6p16, PINCONF_GPIO6p17, PINCONF_GPIO6p18, PINCONF_GPIO6p19, PINCONF_GPIO6p20, PINCONF_GPIO6p21, PINCONF_GPIO6p22, PINCONF_GPIO6p23,
        PINCONF_GPIO6p24, PINCONF_GPIO6p25, PINCONF_GPIO6p26, PINCONF_GPIO6p27, PINCONF_GPIO6p28, PINCONF_GPIO6p29, PINCONF_GPIO6p30,
        0
    }, //port 6

    {
        PINCONF_GPIO7p0, PINCONF_GPIO7p1,  PINCONF_GPIO7p2,  PINCONF_GPIO7p3,  PINCONF_GPIO7p4,  PINCONF_GPIO7p5,  PINCONF_GPIO7p6,  PINCONF_GPIO7p7,
        PINCONF_GPIO7p8,  PINCONF_GPIO7p9,  PINCONF_GPIO7p10, PINCONF_GPIO7p11, PINCONF_GPIO7p12, PINCONF_GPIO7p13, PINCONF_GPIO7p14, PINCONF_GPIO7p15,
        PINCONF_GPIO7p16, PINCONF_GPIO7p17, PINCONF_GPIO7p18, PINCONF_GPIO7p19, PINCONF_GPIO7p20, PINCONF_GPIO7p21, PINCONF_GPIO7p22, PINCONF_GPIO7p23,
        PINCONF_GPIO7p24, PINCONF_GPIO7p25,
        0, 0, 0, 0, 0, 0
    } //port 7
};

// given the physical port and pin (P2.7) finds the GPIO port and pin (GPIO0[7])
static bool lookup_pin(uint16_t port, uint16_t pin, uint16_t& gpio_port, uint16_t& gpio_pin)
{
    for (int i = 0; i < NUM_GPIO_PORTS; ++i) {
        for (int j = 0; j < NUM_GPIO_PINS; ++j) {
            uint32_t v = port_pin_lut[i][j];
            if(v == 0) continue;
            if( ((v & PINCONF_PINS_MASK) >> PINCONF_PINS_SHIFT) == port && ((v & PINCONF_PIN_MASK) >> PINCONF_PIN_SHIFT) == pin ) {
                gpio_port = i;
                gpio_pin = j;
                return true;
            }
        }
    }

    return false;
}

// Make a new pin object from a string
// Pins are defined for the LPC43xx as GPIO names GPIOp[n] or gpiop_n where p is the GPIO port and n is the pin or as pin names eg P1_6 or P1.6
Pin* Pin::from_string(std::string value)
{
    valid = false;
    inverting = false;
    open_drain = false;

    if(value == "nc") return nullptr;

    uint16_t port = 0;
    uint16_t pin = 0;
    size_t pos = 0;
    if(stringutils::toUpper(value.substr(0, 4)) == "GPIO") {
        // grab first integer as GPIO port.
        port = strtol(value.substr(4).c_str(), nullptr, 10);
        pos = value.find_first_of("[_.", 4);
        if(pos == std::string::npos) return nullptr;

        // grab pin number
        pin = strtol(value.substr(pos + 1).c_str(), nullptr, 10);

    } else if(stringutils::toUpper(value.substr(0, 1)) == "P") {
        uint16_t x = strtol(value.substr(1).c_str(), nullptr, 16);
        pos = value.find_first_of("._", 1);
        if(pos == std::string::npos) return nullptr;
        uint16_t y = strtol(value.substr(pos + 1).c_str(), nullptr, 10);

        // Pin name convert to GPIO
        if(!lookup_pin(x, y, port, pin)) return nullptr;

    } else {
        return nullptr;
    }

    if(port >= NUM_GPIO_PORTS || pin >= NUM_GPIO_PINS) return nullptr;

    if(!set_allocated(port, pin)) {
        printf("WARNING: GPIO%d[%d] has already been allocated\n", port, pin);
    }

    // convert port and pin to a GPIO and setup as a GPIO
    uint32_t gpio = port_pin_lut[port][pin];
    if(gpio == 0) return nullptr; // not a valid pin

    // now check for modifiers:-
    // ! = invert pin
    // o = set pin to open drain
    // ^ = set pin to pull up
    // v = set pin to pull down
    // - = set pin to no pull up or down
    // default to pull up for input pins, neither for output
    gpio |= PINCONF_PULLUP;
    for(char c : value.substr(pos + 1)) {
        switch(c) {
            case '!':
                this->inverting = true;
                break;
            case 'o':
                this->open_drain= true; // we need to set pin to input when off for simulated opendrain
                break;
            case '^':
                gpio |= PINCONF_PULLUP;
                break;
            case 'v':
                gpio |= PINCONF_PULLDOWN;
                break;
            case '-':
            	gpio &= ~(PINCONF_PULLUP|PINCONF_PULLDOWN); // clear both bits
                break;
        }
    }
    config_pin(gpio); //configures pin for GPIO

    // save the gpio port and pin (we can always get the pin number from this and the lut)
    gpioport = port;
    gpiopin = pin;

    this->valid = true;
    return this;
}

std::string Pin::to_string() const
{
    if(valid) {
        std::string s("gpio");
        s.append(std::to_string(gpioport)).append("_").append(std::to_string(gpiopin));

        uint32_t v = port_pin_lut[gpioport][gpiopin];
        uint16_t port = ((v & PINCONF_PINS_MASK) >> PINCONF_PINS_SHIFT);
        uint16_t pin = ((v & PINCONF_PIN_MASK) >> PINCONF_PIN_SHIFT);
        const char *digits = "0123456789abcdef";
        s.append("(p");
        s.push_back(digits[port]);
        s.push_back('_');
        s.append(std::to_string(pin)).append(")");
        if(open_drain) s.push_back('o');
        if(inverting) s.push_back('!');
        if(get()) {
        	s.append(":1");
        }else{
        	s.append(":0");
        }
        return s;

    } else {
        return "nc";
    }
}

Pin* Pin::as_output()
{
    if(valid) {
        Chip_GPIO_SetPinDIROutput(LPC_GPIO_PORT, gpioport, gpiopin);
        return this;
    }

    return nullptr;
}

Pin* Pin::as_input()
{
    if(valid) {
        Chip_GPIO_SetPinDIRInput(LPC_GPIO_PORT, gpioport, gpiopin);
        return this;
    }

    return nullptr;
}

/*
 * based on nuttx lpc43_pin_config and the PINCONF #defines above
 *   Copyright (C) 2012 Gregory Nutt. All rights reserved.
 *   Author: Gregory Nutt <gnutt@nuttx.org>
 *
 * Modified to work with lpcopen.
*/
bool Pin::config_pin(uint32_t pinconf)
{
    unsigned int port = ((pinconf & PINCONF_PINS_MASK) >> PINCONF_PINS_SHIFT);
    unsigned int pin    = ((pinconf & PINCONF_PIN_MASK) >> PINCONF_PIN_SHIFT);
    unsigned int func   = ((pinconf & PINCONF_FUNC_MASK) >> PINCONF_FUNC_SHIFT);
    uint16_t regval = 0;

    /* Set up common pin configurations */

    regval = func;

    /* Enable/disable pull-down resistor */

    if (PINCONF_IS_PULLDOWN(pinconf)) {
        regval |= SCU_MODE_PULLDOWN;  /* Set bits to enable */
    }

    if (!PINCONF_IS_PULLUP(pinconf)) {
        regval |= SCU_MODE_INACT; /* Set bit to disable */
    }

    /* Enable/disable input buffering */

    if (PINCONF_INBUFFER_ENABLED(pinconf)) {
        regval |= SCU_MODE_INBUFF_EN; /* Set bit to enable */
    }

    /* Enable/disable glitch filtering */

    if (!PINCONF_GLITCH_ENABLE(pinconf)) {
        regval |= SCU_MODE_ZIF_DIS; /* Set bit to disable */
    }

    /* Only normal and high speed pins support the slew rate setting */

    if (PINCONF_IS_SLEW_FAST(pinconf)) {
        regval |= SCU_MODE_HIGHSPEEDSLEW_EN; /* 0=slow; 1=fast */
    }

    /* Only high drive pins suppose drive strength */

    switch (pinconf & PINCONF_DRIVE_MASK) {
        default:
        case PINCONF_DRIVE_NORMAL: /* Normal-drive: 4 mA drive strength (or not high drive pin) */
            regval |= SCU_MODE_4MA_DRIVESTR;
            break;

        case PINCONF_DRIVE_MEDIUM: /* Medium-drive: 8 mA drive strength */
            regval |= SCU_MODE_8MA_DRIVESTR;
            break;

        case PINCONF_DRIVE_HIGH: /* High-drive: 14 mA drive strength */
            regval |= SCU_MODE_14MA_DRIVESTR;
            break;

        case PINCONF_DRIVE_ULTRA: /* Ultra high-drive: 20 mA drive strength */
            regval |= SCU_MODE_20MA_DRIVESTR;
            break;
    }

    Chip_SCU_PinMuxSet(port, pin, regval);

    return true;
}

#if 0
mbed::InterruptIn* Pin::interrupt_pin()
{
    if(!this->valid) return nullptr;
    /*
        // set as input
        as_input();

        if (port_number == 0 || port_number == 2) {
            PinName pinname = port_pin((PortName)port_number, pin);
            return new mbed::InterruptIn(pinname);

        }else{
            this->valid= false;
            return nullptr;
        }
    */
    return nullptr;
}
#endif
