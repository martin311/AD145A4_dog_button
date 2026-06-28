
  local_irq_enable = ABSOLUTE(0xc2f20);
  local_irq_disable = ABSOLUTE(0xc2f24);
  bit_set_swi = ABSOLUTE(0xc2f28);
  bit_clr_swi = ABSOLUTE(0xc2f2c);
  p33_buf = ABSOLUTE(0xc2f30);
  _p33_xor_1byte = ABSOLUTE(0xc2f34);
  _p33_and_1byte = ABSOLUTE(0xc2f38);
  _p33_or_1byte = ABSOLUTE(0xc2f3c);
  _p33_tx_1byte = ABSOLUTE(0xc2f40);
  _p33_rx_1byte = ABSOLUTE(0xc2f44);
  _P33_CON_SET = ABSOLUTE(0xc2f48);
  memmem = ABSOLUTE(0xc2f4c);
  memcpy = ABSOLUTE(0xc2f50);
  memmove = ABSOLUTE(0xc2f54);
  memcmp = ABSOLUTE(0xc2f58);
  memset = ABSOLUTE(0xc2f5c);
  strcmp = ABSOLUTE(0xc2f60);
  strcpy = ABSOLUTE(0xc2f64);
  strlen = ABSOLUTE(0xc2f68);
  strncmp = ABSOLUTE(0xc2f6c);
  strstr = ABSOLUTE(0xc2f70);
  strchr = ABSOLUTE(0xc2f74);
  cbuf_read = ABSOLUTE(0xc2f78);
  _cbuf_write = ABSOLUTE(0xc2f7c);
  cbuf_is_write_able = ABSOLUTE(0xc2f80);
  cbuf_write_alloc = ABSOLUTE(0xc2f84);
  cbuf_write_updata = ABSOLUTE(0xc2f88);
  cbuf_read_alloc = ABSOLUTE(0xc2f8c);
  cbuf_read_updata = ABSOLUTE(0xc2f90);
  cbuf_init = ABSOLUTE(0xc2f94);
  cbuf_clear = ABSOLUTE(0xc2f98);
  cbuf_rewrite = ABSOLUTE(0xc2f9c);
  sys_clock_init = ABSOLUTE(0xc2fa0);
  clk_out_pa3 = ABSOLUTE(0xc2fa4);
  _sfc_suspend = ABSOLUTE(0xc2fa8);
  sfc_resume = ABSOLUTE(0xc2fac);
  sfc_drop_cache = ABSOLUTE(0xc2fb0);
  chip_crc16 = ABSOLUTE(0xc2fb4);
  CrcDecode = ABSOLUTE(0xc2fb8);
  mask_init = ABSOLUTE(0xc2fbc);
  wdt_clear = ABSOLUTE(0xc2fc0);
  nvram_set_boot_state = ABSOLUTE(0xc2fc4);
  chip_reset = ABSOLUTE(0xc2fc8);
  flash_poweron = ABSOLUTE(0xc2fcc);
  exception_irq_handler = ABSOLUTE(0xc2fd0);
  print = ABSOLUTE(0xc2fd4);
  sprintf = ABSOLUTE(0xc2fd8);
  xprintf = ABSOLUTE(0xc2fdc);
  puts = ABSOLUTE(0xc2fe0);
  doe = ABSOLUTE(0xc2fe4);
  LZ4_setStreamDecode = ABSOLUTE(0xc2fe8);
  LZ4_decompress_safe_continue = ABSOLUTE(0xc2fec);
  LZ4_decompress_generic = ABSOLUTE(0xc2ff0);
  LZ4_decompress_safe_usingDict = ABSOLUTE(0xc2ff4);
  uart_set_dma_dir = ABSOLUTE(0xc2ff8);
  uart_tx_data = ABSOLUTE(0xc2ffc);
  _mdelay = ABSOLUTE(0xc1220);
  _udelay = ABSOLUTE(0xc1114);
  boot_arg_list = ABSOLUTE(0x7ed0);
  htc_pll_ds = ABSOLUTE(0x7ee0);
  _IRQ_MEM_ADDR = ABSOLUTE(0x7f00);
  _MASK_MEM_BEGIN = ABSOLUTE(0x7eb0);
  _MASK_MEM_SIZE = ABSOLUTE(0x48);


MEMORY
{
 app_code(rx) : ORIGIN = 0x100100, LENGTH = 32M
    ram0(rw) : ORIGIN = 0x0, LENGTH = 32K-0x100-0x50 - 0x24
    boot_ram(rw) : ORIGIN = 32k-0x100-0x50-0x24, LENGTH = 0x24
}

ENTRY(_start)

SECTIONS
{


    . = ORIGIN(boot_ram);
    .boot_data ALIGN(4):
    {
         *(.boot_info)
    } > boot_ram

    . = ORIGIN(ram0);
    .data ALIGN(4):
    {
        PROVIDE(data_buf_start = .);
        *(.data*)
  *(.*.data)
        *(.common)
        *(.mic_capless_tab)
        *(.ram_code)
  *(.volatile_ram_code)


        *(.*.text.cache.L1)
        *(.*.text.cache.L2)
        *(.*.text.cache.L3)
    } > ram0

    .ans_data_sec ALIGN(4):
    {
        ans_data_start = .;
        *(.aec_data)
        *(.ns_data)
        *(.nlp_data)
        *(.fft_data)
        *(.noisegate_data)
        ans_data_end = .;
    } > ram0

    .lowpower_overlay ALIGN(4):
    {
        power_driver_data_start = .;
        *(.power_driver.data.overlay)
        *(.power_driver.text.cache.L1.overlay)
        *(.power_driver.data.bss.overlay)
        power_driver_data_end = .;
    } > ram0

 .debug_data ALIGN(4):
 {
        PROVIDE(debug_buf_start = .);
        *(.debug_bss)
        *(.debug_data)
    } > ram0
    .bss (NOLOAD) : SUBALIGN(4)
    {
        PROVIDE(bss_buf_start = .);
        . = ALIGN(32);
        *(.intr_stack)
        . = ALIGN(32);
  *(.stack_magic);
        . = ALIGN(32);
        *(.stack)
        . = ALIGN(32);
  *(.stack_magic0);
        . = ALIGN(32);
  _system_data_begin = .;
        *(.bss)
        *(.*.data.bss)
     *(.non_volatile_ram)
        _system_data_end = .;
        *(.usb_h_dma)
        *(.DAC_BUFFER)
        *(.AUDIO_ADC_BUFFER)

        . = ALIGN(32);
    } > ram0

    .ans_bss_sec ALIGN(4):
    {
        ans_bss_start = .;
        . = ALIGN(32);
        *(.aec_bss)
        *(.ns_bss)
        *(.nlp_bss)
        *(.fft_bss)
        ans_bss_end = .;
    } > ram0


    .effect_buf ALIGN(4):
    {
        PROVIDE(effect_buf_start = .);


        . = ALIGN(4);
        *(.rs_data)


    } > ram0
    OVERLAY : AT(0x4000000)
    {
        .d_toy_music
        {
            PROVIDE(toy_music_buf_start = .);
            *(.toy_music_data);
            PROVIDE(toy_music_buf_end = .);
        }
        d_speed
        {
            . = toy_music_buf_end;
            PROVIDE(speed_buf_start = .);
            . = ALIGN(4);
            *(.sp_data)
            PROVIDE(speed_buf_end = .);
        }
        .d_a
        {
            . = speed_buf_end;
            PROVIDE(a_buf_start = .);
            *(.a_data);
            PROVIDE(a_buf_end = .);
        }

        .d_midi
        {
            . = a_buf_end;
            PROVIDE(midi_buf_start = .);
            *(.midi_buf);
            PROVIDE(midi_buf_end = .);
            PROVIDE(midi_ctrl_buf_start = .);
            *(.midi_ctrl_buf);
            PROVIDE(midi_ctrl_buf_end = .);
        }
        .d_ump3
        {
            . = a_buf_end;
            PROVIDE(ump3_buf_start = .);
            *(.ump3_data);
            PROVIDE(ump3_buf_end = .);
        }
        .d_mp3_st
        {
            . = a_buf_end;
            PROVIDE(mp3_st_buf_start = .);
            *(.mp3_st_data);
            PROVIDE(mp3_st_buf_end = .);
        }
        .d_wav
        {
            . = a_buf_end;
            PROVIDE(wav_buf_start = .);
            *(.wav_data);
            PROVIDE(wav_buf_end = .);
        }
        .d_f1a
        {
            . = a_buf_end;
            PROVIDE(f1a_1_buf_start = .);
            *(.f1a_1_buf);
            PROVIDE(f1a_1_buf_end = .);
           PROVIDE(f1a_2_buf_start = .);
            *(.f1a_2_buf);
           PROVIDE(f1a_2_buf_end = .);
        }
        .d_rec
        {
            *(.rec_data)
            *(.ans_data)
            rec_data_end = .;
        }
        .d_enc_ima
        {
            . = rec_data_end;
            *(.enc_a_data)
        }
        .d_enc_mp3
        {
            . = rec_data_end;
            *(.enc_mp3_data)
        }
        .d_aux
        {

            *(.aux_data)
            aux_data_end = .;
        }
        .d_vp_data0
        {

            *(.speaker_data)
            speaker_data_end = .;
     . = ALIGN(4);
   *(.howling_data)
     . = ALIGN(4);
   *(.notch_howling_data)
     . = ALIGN(4);
     *(.notch_howling_lib)
            howling_data_end = .;
        }

        .d_vp_data1
        {
            . = howling_data_end;
     . = ALIGN(4);
            *(.vp_data);
        }

  .d_vc_data
  {
   . = howling_data_end;
     . = ALIGN(4);
   *(.voicechanger_data);
  }
        .d_echo_data
        {

            . = howling_data_end;
     . = ALIGN(4);
            *(.echo_data);
        }

        .d_pcm_eq_data
        {
            . = howling_data_end;
     . = ALIGN(4);
            *(.pcm_eq_data);
        }

        .pc_buffer
        {
            *(.usb_msd_dma)
            *(.usb_hid_dma )
            *(.usb_config_var)
            *(.mass_storage)
            *(.usb_iso_dma)
            *(.uac_var)
            *(.uac_rx)
            pc_data_end = .;
        }
        .norflash_cache
        {
            . = pc_data_end;
   *(.norflash_cache_buf)
        }

    } > ram0

    d_dec_0 = MAX(midi_ctrl_buf_end,ump3_buf_end);
    d_dec_1 = MAX(d_dec_0 ,mp3_st_buf_end);
    d_dec_2 = MAX(d_dec_1 ,wav_buf_end);
    d_dec_max = MAX(d_dec_2 ,f1a_2_buf_end);


    .heap_buf ALIGN(4):
    {
        PROVIDE(_free_start = .);
        . = LENGTH(ram0) + ORIGIN(ram0) - 1;
        PROVIDE(_free_end = .);
    } > ram0

    _ram_end = .;


    . = ORIGIN(app_code);
    .app_code ALIGN(32):
    {
        *startup.o(.text)
        . = ALIGN(32);
        *(*.f1a_code)
  _VERSION_BEGIN = .;
        KEEP(*(.version))
  _VERSION_END = .;
        *(.debug)
        *(.debug_const)
        *(.debug_code)
        *(.debug_string)
        . = ALIGN(4);

        *(.fft_const)
        *(.fft_code)


        *(*.text.const)
        *(.*.text.const)
        *(*.text)
        *(.text)
        *(.text.*)
        *(.app_root)
        *(.vm)
        . = ALIGN(32);
        _SPI_CODE_START = .;
        *(.spi_code)
        . = ALIGN(4);
        _SPI_CODE_END = .;
        *(.rodata*)
        *(.ins)

        . = ALIGN(4);
  loop_detect_handler_begin = .;
   KEEP(*(.loop_detect_region))
  loop_detect_handler_end = .;

        . = ALIGN(4);
        device_node_begin = .;
        PROVIDE(device_node_begin = .);
        KEEP(*(.device))
        _device_node_end = .;
        PROVIDE(device_node_end = .);

  vfs_ops_begin = .;
  KEEP(*(.vfs_operations))
  vfs_ops_end = .;

  . = ALIGN(4);
     lp_target_begin = .;
     PROVIDE(lp_target_begin = .);
     KEEP(*(.lp_target))
     lp_target_end = .;
     PROVIDE(lp_target_end = .);


     app_size = .;


        . = ALIGN(32);
        text_end = .;
 } >app_code


    bss_begin = ADDR(.bss);
    bss_size = SIZEOF(.bss);


    bss_size1 = _system_data_end - _system_data_begin;
    bss_begin1 = _system_data_begin;

 data_addr = ADDR(.data) ;
 data_begin = text_end ;
 data_size = SIZEOF(.data) + SIZEOF(.debug_data);

    text_size = SIZEOF(.app_code);

    _sdk_text_addr = ADDR(.app_code);
    _sdk_text_size = text_size;
    _sdk_data_size = data_size;


}


lowpower_overlay_addr = power_driver_data_start;
lowpower_overlay_begin = data_begin + data_size;
lowpower_overlay_size = power_driver_data_end - power_driver_data_start;
