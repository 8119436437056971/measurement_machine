#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>

#include "pico/stdlib.h"
#include "pico/multicore.h"

#include "hardware/pio.h"
#include "hardware/timer.h"
#include "hardware/gpio.h"
#include "pio_sample.pio.h"

#include "hardware/pll.h"
#include "hardware/clocks.h"
#include "hardware/structs/clocks.h"

#include "hardware/irq.h"
// -- UART
#define UART_ID uart0
#define BAUD_RATE 115200

// We are using pins 0 and 1, but see the GPIO function select table in the
// datasheet for information on which other pins can be used.
#define UART_TX_PIN 0
#define UART_RX_PIN 1

// -- PIN definitions

const int PIN_DATA = 20;
const int PIN_PULSE = 18;
const int PIN_SYNC = 19;

const int TRIGGER_PIN = 15;

const uint PIN_I_SYNC = 22;
const uint PIN_I_DATA = 21;

const uint LED_PIN = PICO_DEFAULT_LED_PIN;

// --
const uint32_t REPEAT_RATE_FAST_US = 8'000;
const uint32_t REPEAT_RATE_SLOW_US = 800'000;

const uint32_t TRIGGER_PULSE_WIDTH_US = 300;

const uint32_t SLOWDOWN = 1;

class Blink {
public:

	Blink(uint pin, uint32_t delay_us) {
		m_pin = pin;
		m_delay_us = delay_us;

		init();
	}

	void init() {
		gpio_init(m_pin);
		gpio_set_dir(m_pin, GPIO_OUT);
	}

	void loop() {
		if (m_last_loop + m_delay_us < time_us_64()) {
			m_last_loop = time_us_64();
			if (toggle) {
				gpio_put(LED_PIN, 1);
				toggle = false;
			} else {
				gpio_put(LED_PIN, 0);
				toggle = true;
			}
		}
	}
private:
	uint m_pin = 0;
	uint64_t m_last_loop = 0;
	uint32_t m_delay_us = 0;
	bool toggle = false;
};

Blink blink { PICO_DEFAULT_LED_PIN, 400'000 };

volatile bool send_report = false;
volatile bool trace_flag = false;
volatile bool compare_flag = false;
volatile bool repeat_slow = true;

void help() {
	printf("# pio test \n");
	printf("#  see https://www.raspberrypi.org/forums/viewtopic.php?f=32&t=308469\n");
	printf("# \n");
	printf("# Pulse generation \n");
	printf("# - data     gpio out 20 pin 26 \n");
	printf("# - sync     gpio out 19 pin 25 \n");
	printf("# - pulse    gpio out 18 pin 24  debug output for PIO commands \n");
	printf("# - trigger  gpio out 15 pin 20  pulse prox 270us at start of sequence \n");
	printf("# \n");

	printf("# Measurement \n");
	printf("# - data     gpio in  21 pin 27 \n");
	printf("# - sync     gpio in  22 pin 29 \n");
	printf("# \n");
	printf("# Other \n");
	printf("# \n");
	printf("# - LED      blinking @2.5Hz 400ms \n");
	printf("# \n");
	printf("# UART commands \n");
	printf("#\n");
	printf("# h:help ");

	if (compare_flag) {
		printf("c:COMPARE ");
	} else {
		printf("c:compare ");
	}

	if (send_report) {
		printf("r:REPORT ");
	} else {
		printf("r:report ");
	}
	if (repeat_slow) {
		printf("s:SLOW ");
	} else {
		printf("s:slow ");
	}

	if (trace_flag) {
		printf("t:TRACE  ");
	} else {
		printf("t:trace  ");
	}
	printf("\n");
	printf("#\n");

	printf("# compare compare generated and received data \n");
	printf("# report  show which data are generated \n");
	printf("# slow    generate slow or fast. slow is 800ms, fast is 8ms \n");
	printf("# trace   display report of received signals with timing \n");
	printf("#\n");
}

volatile uint32_t data_send[8];

void produce_events_pio() {

	gpio_init(TRIGGER_PIN);
	gpio_set_dir(TRIGGER_PIN, GPIO_OUT);

	PIO pio_0 = pio0;
	uint sm_0 = pio_claim_unused_sm(pio_0, true);
	uint offset_0 = pio_add_program(pio_0, &piotest_program);
	//

	pio_sample_program_init(pio_0, sm_0, offset_0, PIN_DATA, PIN_PULSE, PIN_SYNC, SLOWDOWN);

	uint32_t number = 5;

	while (true) {
		const uint64_t t0 = time_us_64();

		gpio_put(TRIGGER_PIN, 1);
		// trigger pulse a short time before first pulse
		sleep_us(50 * SLOWDOWN);

		// generate some repeating pattern
		for (int i = 0; i < 8; i++) {
			data_send[i] = (number + i) & 0b11111;
		}
		number += 1;

		for (int i = 0; i < 8; i++) {
			pio_sm_put_blocking(pio_0, sm_0, data_send[i] << 27);
		}
		if (send_report) {
			printf("    %02x %02x %02x %02x  %02x %02x %02x %02x\n", //
					data_send[0] & 0b11111, data_send[1] & 0b11111, //
					data_send[2] & 0b11111, data_send[3] & 0b11111, //
					data_send[4] & 0b11111, data_send[5] & 0b11111, //
					data_send[6] & 0b11111, data_send[7] & 0b11111);

		}

		{
			const uint64_t t1 = t0 + TRIGGER_PULSE_WIDTH_US * SLOWDOWN;
			while (time_us_64() < t1)
				;
		}
		gpio_put(TRIGGER_PIN, 0);

		//
		// delay time of 8-digit-group
		//
		uint64_t t1 = 0;
		if (repeat_slow) {
			t1 = t0 + REPEAT_RATE_SLOW_US * SLOWDOWN;
		} else {
			t1 = t0 + REPEAT_RATE_FAST_US * SLOWDOWN;
		}
		while (time_us_64() < t1)
			;
	}
}

void core_1() {
	produce_events_pio();
}

const uint IDLE_TIME_US = 6'000;

volatile uint64_t t0 = 0;
volatile uint8_t data_received[8];
volatile uint32_t n_count = 0;

struct record {
	record(uint64_t _t, uint32_t _data, uint32_t _sync) {
		t = _t;
		data = _data;
		sync = _sync;
	}

	uint64_t t;
	uint32_t data;
	uint32_t sync;
};

std::vector<record> trace { };

void gpio_callback(uint gpio, uint32_t events) {

	uint64_t t1 = time_us_64();
	const uint64_t diff = t1 - t0;
	t0 = t1;

	bool data_level = gpio_get(PIN_I_DATA);

	if (trace.size() < 128) {
		trace.push_back( record(t1, (uint32_t) data_level, (uint32_t) gpio_get(PIN_I_SYNC)));
	}

	if (diff > IDLE_TIME_US) {
		n_count = 0;
	}

	if (n_count == 0) {
		for (int i = 0; i < 8; i++) {
			data_received[i] = 0;
		}

		data_received[0] = (data_level ? 1 : 0) << 4;
		n_count += 1;
	} else {
		if (n_count < (8 * 5)) {
			data_received[n_count / 5] += ((data_level ? 1 : 0) << (4 - (n_count % 5)));
			n_count++;
		}
	}
}

int main() {
	stdio_init_all();
	uart_init(UART_ID, BAUD_RATE);

	gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
	gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);

	help();

	multicore_launch_core1(core_1);

	gpio_init(PIN_I_SYNC);
	gpio_set_dir(PIN_I_SYNC, GPIO_IN);

	gpio_init(PIN_I_DATA);
	gpio_set_dir(PIN_I_DATA, GPIO_IN);

	t0 = time_us_64();

	gpio_set_irq_enabled_with_callback(PIN_I_SYNC, GPIO_IRQ_EDGE_RISE, true, &gpio_callback);

	while (true) {

		if (n_count >= 8 * 5) {

			printf(">>> %02x %02x %02x %02x  %02x %02x %02x %02x\n", data_received[0], data_received[1],
					data_received[2], data_received[3], data_received[4], data_received[5], data_received[6],
					data_received[7]);

			uint32_t ints = save_and_disable_interrupts();

			bool match = true;
			for (int i = 0; i < 8; i++) {
				if (data_received[i] != data_send[i]) {
					match = false;
					break;
				}
			}
			if (trace_flag || (compare_flag && !match)) {
				uint64_t t_start = 0;
				if (trace.size() > 0) {
					t_start = trace[0].t;
				}
				for (int i = 0; i < trace.size(); i++) {

					printf("trace [%3d] %4d  d:%d s:%d", i, (uint32_t)(trace[i].t - t_start), trace[i].data,
							trace[i].sync);
					if (i % 5 == 0) {
						printf(" --- %d", i / 5);
					}
					printf("\n");
				}
			}
			trace.clear();
			n_count = 0;

			restore_interrupts(ints);
		}

		blink.loop();

		if (uart_is_readable(uart0)) {
			int ch = uart_getc(uart0);

			if (ch == ' ') {
				send_report = !send_report;
			} else if (ch == 'r') {
				send_report = !send_report;
			} else if (ch == 'h') {
				help();
			} else if (ch == 's') {
				repeat_slow = true;
			} else if (ch == 'f') {
				repeat_slow = false;
			} else if (ch == 't') {
				trace_flag = !trace_flag;
			} else if (ch == 'c') {
				compare_flag = !compare_flag;
			} else {
				help();
			}
		}

	}
}
