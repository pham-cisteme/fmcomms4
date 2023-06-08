	
/***************************************************************************//**
 * @brief main
*******************************************************************************/
int main(void)
{
	int32_t status;
#ifdef XILINX_PLATFORM
	Xil_ICacheEnable();
	Xil_DCacheEnable();
	default_init_param.spi_param.extra = &xil_spi_param;
	default_init_param.spi_param.platform_ops = &xil_platform_ops;
#endif

#ifdef ALTERA_PLATFORM
	default_init_param.spi_param.platform_ops = &altera_platform_ops;

	if (altera_bridge_init()) {
		printf("Altera Bridge Init Error!\n");
		return -1;
	}
#endif

	// NOTE: The user has to choose the GPIO numbers according to desired
	// carrier board.
	default_init_param.gpio_resetb.number = GPIO_RESET_PIN;

#ifdef FMCOMMS5
	default_init_param.gpio_sync.number = GPIO_SYNC_PIN;
	default_init_param.gpio_cal_sw1.number = GPIO_CAL_SW1_PIN;
	default_init_param.gpio_cal_sw2.number = GPIO_CAL_SW2_PIN;
	default_init_param.rx1rx2_phase_inversion_en = 1;
#else
	default_init_param.gpio_sync.number = -1;
	default_init_param.gpio_cal_sw1.number = -1;
	default_init_param.gpio_cal_sw2.number = -1;
#endif

	if (AD9364_DEVICE)
		default_init_param.dev_sel = ID_AD9364;
	if (AD9363A_DEVICE)
		default_init_param.dev_sel = ID_AD9363A;

#if defined FMCOMMS5 || defined ADI_RF_SOM || defined ADI_RF_SOM_CMOS
	default_init_param.xo_disable_use_ext_refclk_enable = 1;
#endif

#ifdef ADI_RF_SOM_CMOS
	default_init_param.swap_ports_enable = 1;
	default_init_param.lvds_mode_enable = 0;
	default_init_param.lvds_rx_onchip_termination_enable = 0;
	default_init_param.full_port_enable = 1;
	default_init_param.digital_interface_tune_fir_disable = 1;
#endif

	ad9361_init(&ad9361_phy, &default_init_param);

	ad9361_set_tx_fir_config(ad9361_phy, tx_fir_config);
	ad9361_set_rx_fir_config(ad9361_phy, rx_fir_config);

#ifdef FMCOMMS5
#ifdef LINUX_PLATFORM
	gpio_init(default_init_param.gpio_sync);
#endif
	default_init_param.id_no = SPI_CS_2;
	default_init_param.gpio_resetb.number = GPIO_RESET_PIN_2;
#ifdef LINUX_PLATFORM
	gpio_init(default_init_param.gpio_resetb);
#endif
	default_init_param.gpio_sync.number = -1;
	default_init_param.gpio_cal_sw1.number = -1;
	default_init_param.gpio_cal_sw2.number = -1;
	default_init_param.rx_synthesizer_frequency_hz = 2300000000UL;
	default_init_param.tx_synthesizer_frequency_hz = 2300000000UL;

	rx_adc_init.base = AD9361_RX_1_BASEADDR;
	tx_dac_init.base = AD9361_TX_1_BASEADDR;

	ad9361_init(&ad9361_phy_b, &default_init_param);

	ad9361_set_tx_fir_config(ad9361_phy_b, tx_fir_config);
	ad9361_set_rx_fir_config(ad9361_phy_b, rx_fir_config);
#endif
	status = axi_dmac_init(&tx_dmac, &tx_dmac_init);
	if (status < 0) {
		printf("axi_dmac_init tx init error: %"PRIi32"\n", status);
		return status;
	}
	status = axi_dmac_init(&rx_dmac, &rx_dmac_init);
	if (status < 0) {
		printf("axi_dmac_init rx init error: %"PRIi32"\n", status);
		return status;
	}
#ifndef AXI_ADC_NOT_PRESENT
#if defined XILINX_PLATFORM || defined LINUX_PLATFORM || defined ALTERA_PLATFORM
#ifdef DAC_DMA_EXAMPLE
#ifdef FMCOMMS5
	axi_dac_init(&ad9361_phy->tx_dac, &tx_dac_init);
	axi_dac_set_datasel(ad9361_phy_b->tx_dac, -1, AXI_DAC_DATA_SEL_DMA);
#endif
	axi_dac_init(&ad9361_phy->tx_dac, &tx_dac_init);
	axi_dac_set_datasel(ad9361_phy->tx_dac, -1, AXI_DAC_DATA_SEL_DMA);
	axi_dac_set_sine_lut(ad9361_phy->tx_dac, DAC_DDR_BASEADDR);
#else
#ifdef FMCOMMS5
	axi_dac_init(&ad9361_phy_b->tx_dac, &tx_dac_init);
	axi_dac_set_datasel(ad9361_phy_b->tx_dac, -1, AXI_DAC_DATA_SEL_DDS);
#endif
	axi_dac_init(&ad9361_phy->tx_dac, &tx_dac_init);
	axi_dac_set_datasel(ad9361_phy->tx_dac, -1, AXI_DAC_DATA_SEL_DDS);
#endif
#endif
#endif

#ifdef FMCOMMS5
	ad9361_do_mcs(ad9361_phy, ad9361_phy_b);
#endif

#ifndef AXI_ADC_NOT_PRESENT
#if (defined XILINX_PLATFORM || defined ALTERA_PLATFORM) && \
	(defined ADC_DMA_EXAMPLE)
	uint32_t samples = 16384;
#if (defined ADC_DMA_IRQ_EXAMPLE)
	/**
	 * Xilinx platform dependent initialization for IRQ.
	 */
	struct xil_irq_init_param xil_irq_init_par = {
		.type = IRQ_PS,
	};

	/**
	 * IRQ initial configuration.
	 */
	struct irq_init_param irq_init_param = {
		.irq_ctrl_id = INTC_DEVICE_ID,
		.extra = &xil_irq_init_par,
	};

	/**
	 * IRQ instance.
	 */
	struct irq_ctrl_desc *irq_desc;

	status = irq_ctrl_init(&irq_desc, &irq_init_param);
	if(status < 0)
		return status;

	status = irq_global_enable(irq_desc);
	if (status < 0)
		return status;

	struct callback_desc rx_dmac_callback = {
		.ctx = rx_dmac,
		.callback = axi_dmac_default_isr,
		.config = NULL
	};

	status = irq_register_callback(irq_desc,
				       XPAR_FABRIC_AXI_AD9361_ADC_DMA_IRQ_INTR, &rx_dmac_callback);
	if(status < 0)
		return status;

	status = irq_trigger_level_set(irq_desc,
				       XPAR_FABRIC_AXI_AD9361_ADC_DMA_IRQ_INTR, IRQ_LEVEL_HIGH);
	if(status < 0)
		return status;

	status = irq_enable(irq_desc, XPAR_FABRIC_AXI_AD9361_ADC_DMA_IRQ_INTR);
	if(status < 0)
		return status;

	samples = 2097150;
#endif
	// NOTE: To prevent unwanted data loss, it's recommended to invalidate
	// cache after each adc_capture() call, keeping in mind that the
	// size of the capture and the start address must be alinged to the size
	// of the cache line.
	mdelay(1000);

#ifdef DAC_DMA_EXAMPLE
	struct callback_desc tx_dmac_callback = {
		.ctx = tx_dmac,
		.callback = axi_dmac_default_isr,
		.config = NULL
	};

	status = irq_register_callback(irq_desc,
				       XPAR_FABRIC_AXI_AD9361_DAC_DMA_IRQ_INTR, &tx_dmac_callback);
	if(status < 0)
		return status;

	status = irq_enable(irq_desc, XPAR_FABRIC_AXI_AD9361_DAC_DMA_IRQ_INTR);
	if(status < 0)
		return status;

	axi_dmac_transfer_nonblocking(tx_dmac, DAC_DDR_BASEADDR, samples * 16);
#endif
	axi_dmac_transfer(rx_dmac, ADC_DDR_BASEADDR, samples * 16);
#ifdef XILINX_PLATFORM
#ifdef FMCOMMS5
	Xil_DCacheInvalidateRange(ADC_DDR_BASEADDR, samples * 16);
#else
	Xil_DCacheInvalidateRange(ADC_DDR_BASEADDR,
				  ad9361_phy->pdata->rx2tx2 ? samples * 8 : samples * 4);
#endif
#endif
#endif
#endif

#ifdef IIO_SUPPORT

	/**
	 * iio application configurations.
	 */
	struct iio_init_param iio_init_par;

	/**
	 * iio axi adc configurations.
	 */
	struct iio_axi_adc_init_param iio_axi_adc_init_par;

	/**
	 * iio axi dac configurations.
	 */
	struct iio_axi_dac_init_param iio_axi_dac_init_par;

	/**
	 * iio ad9361 configurations.
	 */
	struct iio_ad9361_init_param iio_ad9361_init_param;


	/**
	 * iio application instance descriptor.
	 */
	struct iio_desc *iio_app_desc;

	/**
	 * iio instance descriptor.
	 */
	struct iio_axi_adc_desc *iio_axi_adc_desc;

	/**
	 * iio instance descriptor.
	 */
	struct iio_axi_dac_desc *iio_axi_dac_desc;

	/**
	 * iio ad9361 instance descriptor.
	 */
	struct iio_ad9361_desc *iio_ad9361_desc;

	/**
	 * Xilinx platform dependent initialization for IRQ.
	 */
	struct xil_irq_init_param xil_irq_init_par = {
		.type = IRQ_PS,
	};

	/**
	 * IRQ initial configuration.
	 */
	struct irq_init_param irq_init_param = {
		.irq_ctrl_id = INTC_DEVICE_ID,
		.extra = &xil_irq_init_par,
	};

	/**
	 * iio devices corresponding to every device.
	 */
	struct iio_device *adc_dev_desc, *dac_dev_desc, *ad9361_dev_desc;

	/**
	 * IRQ instance.
	 */
	struct irq_ctrl_desc *irq_desc;

	/**
	 * Xilinx platform dependent initialization for UART.
	 */
	struct xil_uart_init_param xil_uart_init_par;

