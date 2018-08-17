#include <stdlib.h>
#include <stdio.h>

// SPIFI Peripheral
typedef struct SPIFI_t {
    volatile uint32_t CTRL;
    volatile uint32_t CMD;
    volatile uint32_t ADDR;
    volatile uint32_t IDATA;
    volatile uint32_t CLIMIT;
    volatile union {
        volatile uint8_t  DATA8;
        volatile uint16_t DATA16;
        volatile uint32_t DATA32;
    };
    volatile uint32_t MCMD;
    volatile uint32_t STAT;
} SPIFI_t;

#define SPIFI_REG 0x40003000;

#define SPIFI_STAT_MCINIT                    (1 << 0)
#define SPIFI_STAT_CMD                       (1 << 1)
#define SPIFI_STAT_RESET                     (1 << 4)

#define SPIFI_CTRL_TIMEOUT_SHIFT             (0)
#define SPIFI_CTRL_TIMEOUT_MAX               (0xFFFF << SPIFI_CTRL_TIMEOUT_SHIFT)
#define SPIFI_CTRL_CSHIGH_SHIFT              (16)
#define SPIFI_CTRL_RFCLK                     (1 << 29)
#define SPIFI_CTRL_FBCLK                     (1 << 30)

#define SPIFI_CMD_DATALEN_SHIFT              (0)
#define SPIFI_CMD_DOUT_SHIFT                 (15)
#define SPIFI_CMD_DOUT_OUTPUT                (1 << SPIFI_CMD_DOUT_SHIFT)
#define SPIFI_CMD_DOUT_INPUT                 (0 << SPIFI_CMD_DOUT_SHIFT)
#define SPIFI_CMD_INTLEN_SHIFT               (16)
#define SPIFI_CMD_FIELDFORM_SHIFT            (19)
#define SPIFI_CMD_FIELDFORM_ALL_SERIAL       (0 << SPIFI_CMD_FIELDFORM_SHIFT)
#define SPIFI_CMD_FIELDFORM_OPCODE_SERIAL    (2 << SPIFI_CMD_FIELDFORM_SHIFT)
#define SPIFI_CMD_FRAMEFORM_SHIFT            (21)
#define SPIFI_CMD_FRAMEFORM_OPCODE           (1 << SPIFI_CMD_FRAMEFORM_SHIFT)
#define SPIFI_CMD_FRAMEFORM_OPCODE_3ADDRESS  (4 << SPIFI_CMD_FRAMEFORM_SHIFT)
#define SPIFI_CMD_FRAMEFORM_3ADDRESS         (6 << SPIFI_CMD_FRAMEFORM_SHIFT)
#define SPIFI_CMD_OPCODE_SHIFT               (24)


// System Control Unit Peripheral
#define SCU_BASE 0x40086000

#define SFSP3_3 ((volatile uint32_t*)(SCU_BASE + 0x18C))
#define SFSP3_4 ((volatile uint32_t*)(SCU_BASE + 0x190))
#define SFSP3_5 ((volatile uint32_t*)(SCU_BASE + 0x194))
#define SFSP3_6 ((volatile uint32_t*)(SCU_BASE + 0x198))
#define SFSP3_7 ((volatile uint32_t*)(SCU_BASE + 0x19C))
#define SFSP3_8 ((volatile uint32_t*)(SCU_BASE + 0x1A0))

#define SFS_MODE_SHIFT   (0)
#define SFS_MODE_3       (3 << SFS_MODE_SHIFT)
#define SFS_EPUN_SHIFT   (4)
#define SFS_EPUN_DISABLE (1 << SFS_EPUN_SHIFT)  // Disable pull-down
#define SFS_EHS_SHIFT    (5)
#define SFS_EHS_FAST     (1 << SFS_EHS_SHIFT)
#define SFS_EZI_SHIFT    (6)
#define SFS_EZI_ENABLE   (1 << SFS_EZI_SHIFT)   // Enable input buffer
#define SFS_ZIF_SHIFT    (7)
#define SFS_ZIF_DISABLE  (1 << SFS_ZIF_SHIFT)   // Disable input glitch filter


// Clock Generation Unit Peripheral
#define CGU_IDIVB_CTRL ((volatile uint32_t*)0x4005004C)

#define CGU_IDIV_CTRL_IDIV_SHIFT 2
#define CGU_IDIV_CTRL_AUTOBLOCK (1 << 11)
#define CGU_IDIV_CTRL_CLKSEL_SHIFT 24
#define CGU_IDIV_CTRL_CLKSEL_PLL1 (0x9 << CGU_IDIV_CTRL_CLKSEL_SHIFT)


__attribute__  ((section (".ramfunctions"))) void configureSPIFI()
{
    // Make sure that all memory reads from FLASH have completed before disabling SPIFI memory mode.
    __asm volatile ("isb" : : : "memory");
    __asm volatile ("dsb" : : : "memory");

    // Disable interrupts since interrupts in FLASH can't fire while we are setting up SPIFI.
    __asm volatile ("cpsid i" : : : "memory");

    SPIFI_t* SPIFI = (SPIFI_t*)SPIFI_REG;

    // Set the SPIFI pins for low-slew high speed output.
    volatile uint32_t* SPIFI_SCK_PIN_CONFIG = SFSP3_3;
    volatile uint32_t* SPIFI_SIO3_PIN_CONFIG = SFSP3_4;
    volatile uint32_t* SPIFI_SIO2_PIN_CONFIG = SFSP3_5;
    volatile uint32_t* SPIFI_MISO_PIN_CONFIG = SFSP3_6;
    volatile uint32_t* SPIFI_MOSI_PIN_CONFIG = SFSP3_7;
    volatile uint32_t* SPIFI_CS_PIN_CONFIG = SFSP3_8;

    *SPIFI_SCK_PIN_CONFIG = SFS_MODE_3 | SFS_EPUN_DISABLE | SFS_EHS_FAST | SFS_EZI_ENABLE | SFS_ZIF_DISABLE;
    *SPIFI_SIO3_PIN_CONFIG = SFS_MODE_3 | SFS_EPUN_DISABLE | SFS_EHS_FAST | SFS_EZI_ENABLE | SFS_ZIF_DISABLE;
    *SPIFI_SIO2_PIN_CONFIG = SFS_MODE_3 | SFS_EPUN_DISABLE | SFS_EHS_FAST | SFS_EZI_ENABLE | SFS_ZIF_DISABLE;
    *SPIFI_MISO_PIN_CONFIG = SFS_MODE_3 | SFS_EPUN_DISABLE | SFS_EHS_FAST | SFS_EZI_ENABLE | SFS_ZIF_DISABLE;
    *SPIFI_MOSI_PIN_CONFIG = SFS_MODE_3 | SFS_EPUN_DISABLE | SFS_EHS_FAST | SFS_EZI_ENABLE | SFS_ZIF_DISABLE;
    *SPIFI_CS_PIN_CONFIG = SFS_MODE_3 | SFS_EPUN_DISABLE | SFS_EHS_FAST | SFS_EZI_ENABLE | SFS_ZIF_DISABLE;

    // Reset the SPIFI peripheral to exit memory mode.
    SPIFI->STAT = SPIFI_STAT_RESET;
    while (SPIFI->STAT & SPIFI_STAT_RESET) {
    }

    // Remember what the command registers looked like before we start using it to query JEDEC ID.
    // We might want to restore it if we don't recognize this device.
    uint32_t spifiOrigCmd = SPIFI->CMD;
    uint32_t spifiOrigMCmd = SPIFI->MCMD;

    // Read out the JEDEC ID.
    const uint32_t FLASH_OPCODE_JEDEC_ID = 0x9F;
    const uint32_t FLASH_OPCODE_FAST_READ = 0xEB;
    const uint32_t WINBOND_MANUFACTURER_ID = 0xEF;
    uint8_t        manufacturerId = 0;
    uint16_t       capacity = 0;
    // Issue read twice since the first read after reset doesn't seem to give good results.
    for (int i = 0 ; i < 2 ; i++) {
        SPIFI->CMD = (3 << SPIFI_CMD_DATALEN_SHIFT) |
                     SPIFI_CMD_DOUT_INPUT |
                     (0 << SPIFI_CMD_INTLEN_SHIFT) |
                     SPIFI_CMD_FIELDFORM_ALL_SERIAL |
                     SPIFI_CMD_FRAMEFORM_OPCODE |
                     (FLASH_OPCODE_JEDEC_ID << SPIFI_CMD_OPCODE_SHIFT);
        manufacturerId = SPIFI->DATA8;
        capacity = SPIFI->DATA8 << 8;
        capacity |= SPIFI->DATA8;
    }

    // Put things back the way they were if we aren't setting up winbond SPIFI FLASH.
    if (manufacturerId != WINBOND_MANUFACTURER_ID) {
        // Need to reset the FRAMEFORM since they seem to be zeroed out.
        spifiOrigCmd |= SPIFI_CMD_FRAMEFORM_OPCODE_3ADDRESS;
        spifiOrigMCmd |= SPIFI_CMD_FRAMEFORM_3ADDRESS;

        // Send initial command with opcode and wait for it to complete.
        SPIFI->CMD = spifiOrigCmd;
        while (SPIFI->STAT & SPIFI_STAT_CMD) {
        }

        // Setup the memory command with no opcode to get back into memory mode.
        SPIFI->MCMD = spifiOrigMCmd;
        while (0 == (SPIFI->STAT & SPIFI_STAT_MCINIT)) {
        }

        goto CleanupAndExit;
    }

    // Configure SPIFI for higher clock speed operation.
    // Set maximum chip select timeout.
    //  We don't care about power usage of keeping FLASH enabled while waiting for the next memory request.
    // Set CSHIGH to 1+1=2 since smaller values are faster.
    // Enable the use of falling clock edge for data read and the feedback clock to enable the use of higher clock
    // speeds.
    SPIFI->CTRL = SPIFI_CTRL_TIMEOUT_MAX | (1 << SPIFI_CTRL_CSHIGH_SHIFT) | SPIFI_CTRL_RFCLK | SPIFI_CTRL_FBCLK;

    // Set SPIFI clock to be 1/(1+1) = 1/2 the CPU clock (PLL1).
    *CGU_IDIVB_CTRL = (1 << CGU_IDIV_CTRL_IDIV_SHIFT) | CGU_IDIV_CTRL_AUTOBLOCK | CGU_IDIV_CTRL_CLKSEL_PLL1;

    // Re-issue read commands to get no-opcode memory read mode enabled again.
    // Send initial command with opcode and wait for it to complete.
    SPIFI->CMD = (3 << SPIFI_CMD_INTLEN_SHIFT) |
                 SPIFI_CMD_FIELDFORM_OPCODE_SERIAL |
                 SPIFI_CMD_FRAMEFORM_OPCODE_3ADDRESS |
                 (FLASH_OPCODE_FAST_READ << SPIFI_CMD_OPCODE_SHIFT);
    while (SPIFI->STAT & SPIFI_STAT_CMD) {
    }

    // Setup the memory command with no opcode to get back into memory mode.
    SPIFI->MCMD = (3 << SPIFI_CMD_INTLEN_SHIFT) |
                  SPIFI_CMD_FIELDFORM_OPCODE_SERIAL |
                  SPIFI_CMD_FRAMEFORM_3ADDRESS |
                  (FLASH_OPCODE_FAST_READ << SPIFI_CMD_OPCODE_SHIFT);
    while (0 == (SPIFI->STAT & SPIFI_STAT_MCINIT)) {
    }

CleanupAndExit:
    // Re-enable interrupts now that SPIFLASH is set up for high speed memory operation.
    __asm volatile ("cpsie i" : : : "memory");
}
