
	/**
	 * Initialization for UART.
	 */
	struct uart_init_param uart_init_par;
	struct uart_desc *uart_desc;

	status = irq_ctrl_init(&irq_desc, &irq_init_param);
	if(status < 0)
		return status;

	xil_uart_init_par = (struct xil_uart_init_param) {
		.type = UART_PS,
		.irq_id = UART_IRQ_ID,
		.irq_desc = irq_desc,
	};

	uart_init_par = (struct uart_init_param) {
		.baud_rate = 921600,
		.device_id = UART_DEVICE_ID,
		.extra = &xil_uart_init_par,
	};

	status = irq_global_enable(irq_desc);
	if (status < 0)
		return status;

	status = axi_dmac_init(&tx_dmac, &tx_dmac_init);
	if(status < 0)
		return status;

	status = uart_init(&uart_desc, &uart_init_par);
	if (status < 0)
		return status;

	iio_init_par.phy_type = USE_UART;
	iio_init_par.uart_desc = uart_desc;
	status = iio_init(&iio_app_desc, &iio_init_par);
	if(status < 0)
		return status;

	iio_axi_adc_init_par = (struct iio_axi_adc_init_param) {
		.rx_adc = ad9361_phy->rx_adc,
		.rx_dmac = rx_dmac,
#ifndef PLATFORM_MB
		.dcache_invalidate_range = (void (*)(uint32_t,
						     uint32_t))Xil_DCacheInvalidateRange
#endif
	};

	status = iio_axi_adc_init(&iio_axi_adc_desc, &iio_axi_adc_init_par);
	if(status < 0)
		return status;
	iio_axi_adc_get_dev_descriptor(iio_axi_adc_desc, &adc_dev_desc);
	struct iio_data_buffer read_buff = {
		.buff = (void *)ADC_DDR_BASEADDR,
		.size = 0xFFFFFFFF,
	};
	status = iio_register(iio_app_desc, adc_dev_desc, "cf-ad9361-lpc",
			      iio_axi_adc_desc, &read_buff, NULL);
	if (status < 0)
		return status;

	iio_axi_dac_init_par = (struct iio_axi_dac_init_param) {
		.tx_dac = ad9361_phy->tx_dac,
		.tx_dmac = tx_dmac,
#ifndef PLATFORM_MB
		.dcache_flush_range = (void (*)(uint32_t, uint32_t))Xil_DCacheFlushRange,
#endif
	};

	status = iio_axi_dac_init(&iio_axi_dac_desc, &iio_axi_dac_init_par);
	if (status < 0)
		return status;
	iio_axi_dac_get_dev_descriptor(iio_axi_dac_desc, &dac_dev_desc);

	struct iio_data_buffer write_buff = {
		.buff = (void *)DAC_DDR_BASEADDR,
		.size = 0xFFFFFFFF,
	};
	status = iio_register(iio_app_desc, dac_dev_desc, "cf-ad9361-dds-core-lpc",
			      iio_axi_dac_desc, NULL, &write_buff);
	if (status < 0)
		return status;

	iio_ad9361_init_param = (struct iio_ad9361_init_param) {
		.ad9361_phy = ad9361_phy,
	};

	status = iio_ad9361_init(&iio_ad9361_desc, &iio_ad9361_init_param);
	if (status < 0)
		return status;
	iio_ad9361_get_dev_descriptor(iio_ad9361_desc, &ad9361_dev_desc);
	status = iio_register(iio_app_desc, ad9361_dev_desc, "ad9361-phy", ad9361_phy,
			      NULL, NULL);
	if (status < 0)
		return status;

	do {
		status = iio_step(iio_app_desc);
	} while (true);

#endif // IIO_SUPPORT

	printf("Done.\n");

#ifdef TDD_SWITCH_STATE_EXAMPLE
	uint32_t ensm_mode;
	struct gpio_desc 	*gpio_enable_pin;
	struct gpio_desc 	*gpio_txnrx_pin;
	if (!ad9361_phy->pdata->fdd) {
		if (ad9361_phy->pdata->ensm_pin_ctrl) {
			gpio_init.number = GPIO_ENABLE_PIN;
			status = gpio_get(&gpio_enable_pin, gpio_init);
			if (status != SUCCESS) {
				printf("gpio_get() error: %"PRIi32"\n", status);
				return status;
			}
			gpio_direction_output(gpio_enable_pin, 1);
			gpio_init.number = GPIO_TXNRX_PIN;
			status = gpio_get(&gpio_txnrx_pin, gpio_init);
			if (status != SUCCESS) {
				printf("gpio_get() error: %"PRIi32"\n", status);
				return status;
			}
			gpio_direction_output(gpio_txnrx_pin, 0);
			udelay(10);
			ad9361_get_en_state_machine_mode(ad9361_phy, &ensm_mode);
			printf("TXNRX control - Alert: %s\n",
			       ensm_mode == ENSM_MODE_ALERT ? "OK" : "Error");
			mdelay(1000);

			if (ad9361_phy->pdata->ensm_pin_pulse_mode) {
				while(1) {
					gpio_set_value(gpio_txnrx_pin, 0);
					udelay(10);
					gpio_set_value(gpio_enable_pin, 1);
					udelay(10);
					gpio_set_value(gpio_enable_pin, 0);
					ad9361_get_en_state_machine_mode(ad9361_phy, &ensm_mode);
					printf("TXNRX Pulse control - RX: %s\n",
					       ensm_mode == ENSM_MODE_RX ? "OK" : "Error");
					mdelay(1000);

					gpio_set_value(gpio_enable_pin, 1);
					udelay(10);
					gpio_set_value(gpio_enable_pin, 0);
					ad9361_get_en_state_machine_mode(ad9361_phy, &ensm_mode);
					printf("TXNRX Pulse control - Alert: %s\n",
					       ensm_mode == ENSM_MODE_ALERT ? "OK" : "Error");
					mdelay(1000);

					gpio_set_value(gpio_txnrx_pin, 1);
					udelay(10);
					gpio_set_value(gpio_enable_pin, 1);
					udelay(10);
					gpio_set_value(gpio_enable_pin, 0);
					ad9361_get_en_state_machine_mode(ad9361_phy, &ensm_mode);
					printf("TXNRX Pulse control - TX: %s\n",
					       ensm_mode == ENSM_MODE_TX ? "OK" : "Error");
					mdelay(1000);

					gpio_set_value(gpio_enable_pin, 1);
					udelay(10);
					gpio_set_value(gpio_enable_pin, 0);
					ad9361_get_en_state_machine_mode(ad9361_phy, &ensm_mode);
					printf("TXNRX Pulse control - Alert: %s\n",
					       ensm_mode == ENSM_MODE_ALERT ? "OK" : "Error");
					mdelay(1000);
				}
			} else {
				while(1) {
					gpio_set_value(gpio_txnrx_pin, 0);
					udelay(10);
					gpio_set_value(gpio_enable_pin, 1);
					udelay(10);
					ad9361_get_en_state_machine_mode(ad9361_phy, &ensm_mode);
					printf("TXNRX control - RX: %s\n",
					       ensm_mode == ENSM_MODE_RX ? "OK" : "Error");
					mdelay(1000);

					gpio_set_value(gpio_enable_pin, 0);
					udelay(10);
					ad9361_get_en_state_machine_mode(ad9361_phy, &ensm_mode);
					printf("TXNRX control - Alert: %s\n",
					       ensm_mode == ENSM_MODE_ALERT ? "OK" : "Error");
					mdelay(1000);

					gpio_set_value(gpio_txnrx_pin, 1);
					udelay(10);
					gpio_set_value(gpio_enable_pin, 1);
					udelay(10);
					ad9361_get_en_state_machine_mode(ad9361_phy, &ensm_mode);
					printf("TXNRX control - TX: %s\n",
					       ensm_mode == ENSM_MODE_TX ? "OK" : "Error");
					mdelay(1000);

					gpio_set_value(gpio_enable_pin, 0);
					udelay(10);
					ad9361_get_en_state_machine_mode(ad9361_phy, &ensm_mode);
					printf("TXNRX control - Alert: %s\n",
					       ensm_mode == ENSM_MODE_ALERT ? "OK" : "Error");
					mdelay(1000);
				}
			}
		} else {
			while(1) {
				ad9361_set_en_state_machine_mode(ad9361_phy, ENSM_MODE_RX);
				ad9361_get_en_state_machine_mode(ad9361_phy, &ensm_mode);
				printf("SPI control - RX: %s\n",
				       ensm_mode == ENSM_MODE_RX ? "OK" : "Error");
				mdelay(1000);

				ad9361_set_en_state_machine_mode(ad9361_phy, ENSM_MODE_ALERT);
				ad9361_get_en_state_machine_mode(ad9361_phy, &ensm_mode);
				printf("SPI control - Alert: %s\n",
				       ensm_mode == ENSM_MODE_ALERT ? "OK" : "Error");
				mdelay(1000);

				ad9361_set_en_state_machine_mode(ad9361_phy, ENSM_MODE_TX);
				ad9361_get_en_state_machine_mode(ad9361_phy, &ensm_mode);
				printf("SPI control - TX: %s\n",
				       ensm_mode == ENSM_MODE_TX ? "OK" : "Error");
				mdelay(1000);

				ad9361_set_en_state_machine_mode(ad9361_phy, ENSM_MODE_ALERT);
				ad9361_get_en_state_machine_mode(ad9361_phy, &ensm_mode);
				printf("SPI control - Alert: %s\n",
				       ensm_mode == ENSM_MODE_ALERT ? "OK" : "Error");
				mdelay(1000);
			}
		}
	}
#endif

	ad9361_remove(ad9361_phy);
#ifdef FMCOMMS5
	ad9361_remove(ad9361_phy_b);
#endif

#ifdef XILINX_PLATFORM
	Xil_DCacheDisable();
	Xil_ICacheDisable();
#endif

#ifdef ALTERA_PLATFORM
	if (altera_bridge_uninit()) {
		printf("Altera Bridge Uninit Error!\n");
		return -1;
	}
#endif

	return 0;
}
