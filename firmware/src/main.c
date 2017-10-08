#include <stdint.h>

typedef struct gpio_port_t {
	volatile uint32_t dir;		/* +0x000 */
	uint32_t _reserved0[31];
	volatile uint32_t mask;		/* +0x080 */
	uint32_t _reserved1[31];
	volatile uint32_t pin;		/* +0x100 */
	uint32_t _reserved2[31];
	volatile uint32_t mpin;		/* +0x180 */
	uint32_t _reserved3[31];
	volatile uint32_t set;		/* +0x200 */
	uint32_t _reserved4[31];
	volatile uint32_t clr;		/* +0x280 */
	uint32_t _reserved5[31];
	volatile uint32_t not;		/* +0x300 */
} gpio_port_t;

#define GPIO_PORT(n) ((gpio_port_t *)(0x400f4000 + 0x2000 + (n) * 4))

void main(void)
{
	gpio_port_t *p_gpio_port;
	int i;

	/* Init all GPIOs as inputs */
	for (i = 0; i < 8; i++) {
		p_gpio_port = GPIO_PORT(i);
		p_gpio_port->dir = 0;
	}

	/* GPIO port 2 pin 1, 2 and 8 as output */
	p_gpio_port = GPIO_PORT(2);
	p_gpio_port->dir |= (1 << 1);
	p_gpio_port->dir |= (1 << 2);
	p_gpio_port->dir |= (1 << 8);

	while(1) {
		/* Wait a bit... */
		for (i = 0; i < 2000000; i++) {
			__asm__("nop");
		}

		p_gpio_port->not |= (1 << 1);

		/* Wait a bit... */
		for (i = 0; i < 2000000; i++) {
			__asm__("nop");
		}

		p_gpio_port->not |= (1 << 2);

		/* Wait a bit... */
		for (i = 0; i < 2000000; i++) {
			__asm__("nop");
		}

		p_gpio_port->not |= (1 << 8);
	}
}
