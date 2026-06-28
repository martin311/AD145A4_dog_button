#include "toy_music.h"
#include "app_modules.h"
#include "simple_play_file.h"
#include "common.h"
#include "msg.h"
#include "key.h"
#include "toy_main.h"
#include "vfs.h"
#include "vm_api.h"
#include "circular_buf.h"
#include "jiffies.h"
#include "tick_timer_driver.h"
#include "device_mge.h"
#include "bsp_loop.h"
#include "toy_record.h"
#include "wdt.h"

#include "decoder_api.h"
#include "decoder_msg_tab.h"
#include "dac_api.h"

#define LOG_TAG_CONST       NORM
#define LOG_TAG             "[toy_music]"
#include "log.h"

#define TFG_EXT_FLASH_EN        1
#define PLAY_KEY_IO             IO_PORTA_03
#define RECORD_KEY_IO           IO_PORTA_02
#define RECORD_LONG_PRESS_TICKS HZ
#define RECORD_MAX_TICKS        (5 * HZ)
#define RECORD_KEY_STABLE_CNT   2

#if SIMPLE_DEC_EN

static play_control dec_pctl[2] AT(.toy_music_data);
static play_control demo_ext_pctl AT(.toy_music_data);
static dp_buff demo_ext_dp AT(.toy_music_data);
static u8 demo_ext_mounted AT(.toy_music_data);
static u8 demo_ext_playing AT(.toy_music_data);
static Encode_Control demo_record_obj;
static dec_obj *demo_record_dec_obj;
static u8 demo_has_record;
static u8 demo_recording;
static u8 demo_record_stop_requested;
static u8 demo_record_playing;
static u32 demo_record_start_jiffies;

#define INR_DIR_NUM   5
static dp_buff inr_dec_dp[INR_DIR_NUM] AT(.toy_music_data);
static const char *const dir_inr_tab[INR_DIR_NUM] = {
    "/dir_song",
    "/dir_eng",
    "/dir_poetry",
    "/dir_story",
    "/dir_bin_f1x",
};
static const u8 dir_inr_vm_tab[INR_DIR_NUM] = {
    VM_INDEX_SONG,
    VM_INDEX_ENG,
    VM_INDEX_POETRY,
    VM_INDEX_STORY,
    VM_INDEX_F1X,
};
static const char *const dir_tab_a[] = {
    "/dir_a",
};

#if TFG_EXT_FLASH_EN
#define EXT_DIR_NUM   1
static dp_buff ext_dec_dp[EXT_DIR_NUM] AT(.toy_music_data);
static const char *const dir_ext_tab[EXT_DIR_NUM] = {
    "/",
};
static const char dir_ext_vm_tab[EXT_DIR_NUM] = {
    VM_INDEX_EXT_SONG,
};
#endif

static void demo_key_init(void)
{
    gpio_set_pull_up(PLAY_KEY_IO, 1);
    gpio_set_pull_down(PLAY_KEY_IO, 0);
    gpio_set_die(PLAY_KEY_IO, 1);
    gpio_set_direction(PLAY_KEY_IO, 1);

    gpio_set_pull_up(RECORD_KEY_IO, 1);
    gpio_set_pull_down(RECORD_KEY_IO, 0);
    gpio_set_die(RECORD_KEY_IO, 1);
    gpio_set_direction(RECORD_KEY_IO, 1);
}

static u8 demo_key_pressed_once(void)
{
    static u8 last_level = 1;
    static u8 stable_level = 1;
    static u8 stable_cnt = 0;
    u8 level = gpio_read(PLAY_KEY_IO);

    if (level == last_level) {
        if (stable_cnt < 5) {
            stable_cnt++;
        }
    } else {
        stable_cnt = 0;
        last_level = level;
        log_info("PA3 raw:%d\n", level);
    }

    if ((stable_cnt >= RECORD_KEY_STABLE_CNT) && (stable_level != level)) {
        stable_level = level;
        if (0 == stable_level) {
            return 1;
        }
    }
    return 0;
}

static void demo_record_stop(void)
{
    if ((!demo_recording) && (!demo_record_stop_requested)) {
        return;
    }

    demo_recording = 0;
    demo_record_stop_requested = 1;
    debug_led_record_set(0);
    log_info("demo record stopping\n");
    record_encode_stop(&demo_record_obj);
    demo_has_record = 1;
    demo_record_stop_requested = 0;
    log_info("demo record stop\n");
}

static void demo_record_start(void)
{
    if (demo_recording || demo_record_stop_requested) {
        return;
    }

    if (demo_record_dec_obj) {
        decoder_stop(demo_record_dec_obj, NEED_WAIT);
        demo_record_dec_obj = NULL;
    }
    if (demo_record_playing) {
        encode_file_fs_close(&demo_record_obj);
        demo_record_playing = 0;
    }
    if (demo_ext_playing) {
        decoder_stop(demo_ext_pctl.p_dec_obj, NEED_WAIT);
        demo_ext_pctl.p_dec_obj = NULL;
        demo_ext_playing = 0;
    }

    encode_file_fs_close(&demo_record_obj);
    memset(&demo_record_obj, 0, sizeof(demo_record_obj));
    if (0 == record_encode_start(&demo_record_obj)) {
        demo_recording = 1;
        demo_record_stop_requested = 0;
        demo_record_start_jiffies = jiffies;
        debug_led_record_set(1);
        log_info("demo record start\n");
    } else {
        debug_led_record_set(0);
        log_error("demo record start err\n");
    }
}

static void demo_record_key_scan(void)
{
    static u8 last_level = 1;
    static u8 stable_level = 1;
    static u8 stable_cnt = 0;
    static u8 long_started = 0;
    static u32 press_jiffies = 0;
    u8 level = gpio_read(RECORD_KEY_IO);

    if (level == last_level) {
        if (stable_cnt < 5) {
            stable_cnt++;
        }
    } else {
        stable_cnt = 0;
        last_level = level;
        log_info("PA2 raw:%d rec:%d\n", level, demo_recording);
    }

    if (demo_recording && (!demo_record_stop_requested) && (1 == level)) {
        demo_record_stop_requested = 1;
        log_info("record key release\n");
        demo_record_stop();
        stable_level = 1;
        long_started = 0;
        return;
    }

    if ((stable_cnt >= RECORD_KEY_STABLE_CNT) && (stable_level != level)) {
        stable_level = level;
        if (0 == stable_level) {
            press_jiffies = jiffies;
            long_started = 0;
        } else {
            if (long_started && (!demo_record_stop_requested)) {
                demo_record_stop_requested = 1;
                demo_record_stop();
            }
            long_started = 0;
        }
    }

    if ((0 == stable_level) && (!long_started) &&
        time_after(jiffies, press_jiffies + RECORD_LONG_PRESS_TICKS)) {
        long_started = 1;
        log_info("record key long\n");
        demo_record_start();
    }

    if (demo_recording && (!demo_record_stop_requested) &&
        time_after(jiffies, demo_record_start_jiffies + RECORD_MAX_TICKS)) {
        demo_record_stop_requested = 1;
        log_info("record max timeout\n");
        demo_record_stop();
    }

    if (demo_recording || demo_record_stop_requested) {
        wdt_clear();
    }
}

static void demo_record_play(void)
{
    if (demo_recording || demo_record_stop_requested) {
        log_info("ignore play while recording\n");
        return;
    }

    if (demo_record_dec_obj) {
        decoder_stop(demo_record_dec_obj, NEED_WAIT);
        demo_record_dec_obj = NULL;
    }
    if (demo_record_playing) {
        encode_file_fs_close(&demo_record_obj);
        demo_record_playing = 0;
        return;
    }
    if (demo_ext_playing) {
        decoder_stop(demo_ext_pctl.p_dec_obj, NEED_WAIT);
        demo_ext_pctl.p_dec_obj = NULL;
        demo_ext_playing = 0;
    }

    if (!demo_has_record) {
        log_info("try latest norfs record\n");
        demo_record_obj.dev_index = EXT_FLASH_RW;
        strcpy(demo_record_obj.fs_name, "norfs");
    }
    demo_record_dec_obj = norfs_enc_file_decode(&demo_record_obj, BIT_A | BIT_UMP3);
    if (NULL != demo_record_dec_obj) {
        demo_has_record = 1;
        demo_record_playing = 1;
        log_info("demo record play\n");
    } else {
        encode_file_fs_close(&demo_record_obj);
        demo_has_record = 0;
        log_error("demo record play err\n");
    }
}

static u32 demo_ext_song_mount(void)
{
    if (demo_ext_mounted) {
        return 0;
    }

    memset(&demo_ext_pctl, 0, sizeof(demo_ext_pctl));
    memset(&demo_ext_dp, 0, sizeof(demo_ext_dp));
    demo_ext_pctl.dev_index = EXT_FLASH_RW;
    demo_ext_pctl.findex = 1;
    demo_ext_pctl.loop = 0;
    demo_ext_pctl.dec_type = BIT_F1A1 | BIT_UMP3;
    demo_ext_pctl.pdp = &demo_ext_dp;
    demo_ext_pctl.pdir = (void *)&dir_ext_tab[0];
    demo_ext_pctl.dir_total = sizeof(dir_ext_tab) / 4;

    if (simple_dev_fs_mount(&demo_ext_pctl)) {
        log_error("demo ext flash mount err\n");
        return -1;
    }

    demo_ext_mounted = 1;
    return 0;
}

static void demo_ext_song_toggle(void)
{
    if (demo_ext_playing) {
        decoder_stop(demo_ext_pctl.p_dec_obj, NEED_WAIT);
        demo_ext_pctl.p_dec_obj = NULL;
        demo_ext_playing = 0;
        log_info("demo ext song stop\n");
        return;
    }

    if (demo_ext_song_mount()) {
        return;
    }

    decoder_stop(dec_pctl[0].p_dec_obj, NEED_WAIT);
    decoder_stop(dec_pctl[1].p_dec_obj, NEED_WAIT);
    demo_ext_pctl.findex = 1;
    if (0 == play_one_file(&demo_ext_pctl)) {
        demo_ext_playing = 1;
        log_info("demo ext song play\n");
    } else {
        demo_ext_playing = 0;
        log_error("demo ext song play err\n");
    }
}

void toy_music_app(void)
{
    log_info("toy_music mode\n");
#if KEY_IR_EN
    Sys_IRInput = 1;
#endif
    int msg[2], err;
    key_table_sel(NULL);
    decoder_init();
    demo_key_init();
    debug_led_record_set(0);

    memset(&dec_pctl[0], 0, sizeof(dec_pctl));      //初始化dec_pctl[0]和dec_pctl[1]
    memset(&inr_dec_dp[0], 0, sizeof(inr_dec_dp));  //初始化dec_dp[0]和dec_dp[1]
    memset(&demo_record_obj, 0, sizeof(demo_record_obj));
    demo_record_dec_obj = NULL;
    demo_has_record = 0;
    demo_recording = 0;
    demo_record_stop_requested = 0;
    demo_record_playing = 0;
    demo_record_start_jiffies = 0;
#if TFG_EXT_FLASH_EN
    memset(&ext_dec_dp[0], 0, sizeof(ext_dec_dp));
#endif

    /* Stage 1 only records and plays the saved recording. Do not mount
     * /dir_song or built-in A resources in this mode. */
#if 0
    dec_pctl[0].dev_index   = INNER_FLASH_RO;
    dec_pctl[0].findex      = 1;
    dec_pctl[0].loop        = 0;
    dec_pctl[0].dec_type    = BIT_F1A1 | BIT_UMP3;
    dec_pctl[0].pdp         = &inr_dec_dp[0];
    dec_pctl[0].p_vm_tab    = (void *)&dir_inr_vm_tab[0];
    dec_pctl[0].pdir        = (void *)&dir_inr_tab[0];
    dec_pctl[0].dir_total   = sizeof(dir_inr_tab) / 4;
    simple_dev_fs_mount(&dec_pctl[0]);
#if SIMPLE_DEC_BP_ENABLE
    /* 读取断点信息 */
    if (NULL != dec_pctl[0].pdp) {
        if (sizeof(dp_buff) == vm_read(\
                                       dec_pctl[0].p_vm_tab[dec_pctl[0].dir_index], \
                                       dec_pctl[0].pdp,                            \
                                       sizeof(dp_buff))) {
            dp_buff *pdp = (dp_buff *)dec_pctl[0].pdp;
            dec_pctl[0].findex = pdp->findex;
        }
    }
#endif

    dec_pctl[1].dev_index   = INNER_FLASH_RO;
    dec_pctl[1].findex      = 1;
    dec_pctl[1].loop        = 0;//A格式无缝循环不需要断点buff
    dec_pctl[1].dec_type    = BIT_A;
    dec_pctl[1].pdir        = (void *)&dir_tab_a[0];
    dec_pctl[1].dir_total   = sizeof(dir_tab_a) / 4;
    simple_dev_fs_mount(&dec_pctl[1]);
#endif

    /* dec_pctl[2].dev_index   = INNER_FLASH_RO; */
    /* dec_pctl[2].findex      = 1; */
    /* dec_pctl[2].dec_type    = BIT_F1A2; */
    /* dec_pctl[2].pdir        = (void *)&dir_inr_tab[0]; */
    /* dec_pctl[2].dir_total   = sizeof(dir_inr_tab) / 4; */
    /* simple_dev_fs_mount(&dec_pctl[2]); */

    /* First-stage flow: do not auto-play when there is no recording. */
    /* post_msg(1, MSG_PLAY_FILE1); */
    /* post_msg(1, MSG_PLAY_FILE2); */
    /* post_msg(1, MSG_A_PLAY); */
    /* simple_play_file_bypath(&dec_pctl[0], "/dir_song/so002.f1b"); */

    while (1) {
        err = get_msg(2, &msg[0]);
        bsp_loop();
        if (MSG_NO_ERROR != err) {
            msg[0] = NO_MSG;
            log_info("get msg err 0x%x\n", err);
        }
        demo_record_key_scan();
        if (!demo_recording && demo_key_pressed_once()) {
            log_info("PA3 play key\n");
            demo_record_play();
        }

        switch (msg[0]) {
        case MSG_PLAY_FILE1:
            log_info("ignore MSG_PLAY_FILE1\n");
            break;

        case MSG_PP:
            log_info("ignore MSG_PP\n");
            break;
        case MSG_PREV_FILE:
            log_info("ignore MSG_PREV_FILE\n");
            break;
        case MSG_NEXT_FILE:
            log_info("ignore MSG_NEXT_FILE\n");
            break;

        case MSG_NEXT_DIR:
            log_info("ignore MSG_NEXT_DIR\n");
            break;

#if TFG_EXT_FLASH_EN
        case MSG_NEXT_DEVICE:
            log_info("ignore MSG_NEXT_DEVICE\n");
            break;
#endif
        case MSG_WFILE_FULL:
            log_info("MSG_WFILE_FULL\n");
            demo_record_stop();
            break;

        case MSG_F1A1_FILE_ERR:
        case MSG_MP3_FILE_ERR:
        case MSG_WAV_FILE_ERR:
        case MSG_A_FILE_ERR:
        case MSG_F1A1_FILE_END:
        case MSG_MP3_FILE_END:
        case MSG_WAV_FILE_END:
        case MSG_A_FILE_END:
            if (demo_record_playing) {
                log_info("demo record end or err\n");
                decoder_stop(demo_record_dec_obj, NEED_WAIT);
                demo_record_dec_obj = NULL;
                encode_file_fs_close(&demo_record_obj);
                demo_record_playing = 0;
                break;
            }
            if (demo_ext_playing) {
                log_info("demo ext song end or err\n");
                decoder_stop(demo_ext_pctl.p_dec_obj, NEED_WAIT);
                demo_ext_pctl.p_dec_obj = NULL;
                demo_ext_playing = 0;
                break;
            }
            log_info("ignore file end or err\n");
            break;

        case MSG_A_PLAY:
            log_info("ignore MSG_A_PLAY\n");
            break;

        /* case MSG_PLAY_FILE2: */
        /*     log_info("MSG_PLAY_F1A2\n"); */
        /*     play_one_file(&dec_pctl[2]); */
        /*     break; */
        /* case MSG_F1A2_FILE_END: */
        /* case MSG_F1A2_FILE_ERR: */
        /*     log_info("F1A2 FILE END OR ERR\n"); */
        /*     decoder_stop(dec_pctl[2].p_dec_obj, NEED_WAIT); */
        /*     break; */

        case MSG_F1A1_LOOP:
        case MSG_F1A2_LOOP:
        case MSG_MP3_LOOP:
        case MSG_WAV_LOOP:
        case MSG_A_LOOP:
            log_info("-loop\n");
            break;

        case MSG_CHANGE_WORK_MODE:
            goto __toy_music_exit;
        case MSG_500MS:
            vm_pre_erase();
            /* Keep the app awake while PA8 is used as a run indicator. */
            /* sys_idle_deal(-2); */
            break;
        default:
            common_msg_deal(&msg[0]);
            break;
        }
    }
__toy_music_exit:
    demo_record_stop();
    if (demo_record_playing) {
        decoder_stop(demo_record_dec_obj, NEED_WAIT);
        demo_record_dec_obj = NULL;
        encode_file_fs_close(&demo_record_obj);
        demo_record_playing = 0;
    }
    debug_led_record_set(0);
    if (demo_ext_playing) {
        decoder_stop(demo_ext_pctl.p_dec_obj, NEED_WAIT);
        demo_ext_playing = 0;
    }
    if (demo_ext_mounted) {
        simple_dev_fs_close(&demo_ext_pctl);
        demo_ext_mounted = 0;
    }
#if KEY_IR_EN
    Sys_IRInput = 0;
#endif
    key_table_sel(NULL);
}



#if TFG_EXT_FLASH_EN
static u32 simple_switch_device(play_control *ppctl)
{
    decoder_stop(ppctl->p_dec_obj, NEED_WAIT);//记录音乐断点
    if (NULL != ppctl->pdp) {
        if (true == get_dp(ppctl->p_dec_obj, ppctl->pdp)) {
#if SIMPLE_DEC_BP_ENABLE
            vm_write(ppctl->p_vm_tab[ppctl->dir_index], ppctl->pdp, sizeof(dp_buff));
#endif
        }
    }
    simple_dev_fs_close(ppctl);
    if (EXT_FLASH_RW == ppctl->dev_index) {
        log_info("SWITCH TO INNER_FLASH\n");
        memset(ppctl, 0, sizeof(play_control));
        ppctl->dev_index   = INNER_FLASH_RO;
        ppctl->findex      = 1;
        ppctl->dec_type    = BIT_F1A1 | BIT_UMP3;
        ppctl->pdp         = &inr_dec_dp[ppctl->dir_index];
        ppctl->p_vm_tab    = (void *)&dir_inr_vm_tab[0];
        ppctl->pdir        = (void *)&dir_inr_tab[0];
        ppctl->dir_total   = sizeof(dir_inr_tab) / 4;
    } else {
        log_info("SWITCH TO EXT_FLASH\n");
        memset(ppctl, 0, sizeof(play_control));
        ppctl->dev_index   = EXT_FLASH_RW;
        ppctl->findex      = 1;
        ppctl->dec_type    = BIT_F1A1 | BIT_UMP3;
        ppctl->pdp         = &ext_dec_dp[ppctl->dir_index];
        ppctl->p_vm_tab    = (void *)&dir_ext_vm_tab[0];
        ppctl->pdir        = (void *)&dir_ext_tab[0];
        ppctl->dir_total   = sizeof(dir_ext_tab) / 4;
    }
    if (simple_dev_fs_mount(ppctl)) {
        log_error("play next dev err!\n");
        return false;
    }
#if SIMPLE_DEC_BP_ENABLE
    if (NULL != ppctl->pdp) {
        u32 ret = vm_read(ppctl->p_vm_tab[ppctl->dir_index], \
                          ppctl->pdp, \
                          sizeof(dp_buff));
        if (sizeof(dp_buff) == ret) {
            dp_buff *dp = (dp_buff *)ppctl->pdp;
            ppctl->findex = dp->findex;
        }
    }
    log_info("next dev findex : %d\n", ppctl->findex);
#endif
    return play_one_file(ppctl);
}
#endif

/*----------------------------------------------------------------------------*/
/**@brief   播放下一个文件夹
   @param   ppctl 播放器句柄
   @return  成功：解码器句柄  失败：NULL
   @note     static dec_obj *play_one_file(music_player *p_music)
*/
/*----------------------------------------------------------------------------*/
static u32 simple_next_dir(play_control *ppctl)
{
    decoder_stop(ppctl->p_dec_obj, NEED_WAIT);//记录音乐断点
    if (NULL != ppctl->pdp) {
        if (true == get_dp(ppctl->p_dec_obj, ppctl->pdp)) {
#if SIMPLE_DEC_BP_ENABLE
            vm_write(ppctl->p_vm_tab[ppctl->dir_index], ppctl->pdp, sizeof(dp_buff));
#endif
        }
    }
    ppctl->dir_index++;
    if (ppctl->dir_index >= ppctl->dir_total) {
        ppctl->dir_index = 0;
    }
#if TFG_EXT_FLASH_EN
    if (EXT_FLASH_RW == ppctl->dev_index) {
        ppctl->pdp = &ext_dec_dp[ppctl->dir_index];
    } else
#endif
    {
        ppctl->pdp = &inr_dec_dp[ppctl->dir_index];
    }
    if (NULL != ppctl->pdp) {
#if SIMPLE_DEC_BP_ENABLE
        vm_read(ppctl->p_vm_tab[ppctl->dir_index], ppctl->pdp, sizeof(dp_buff));
#endif
        dp_buff *dp = (dp_buff *)ppctl->pdp;
        /* log_info("dp->findex : %d\n", dp->findex); */
        if (0 != dp->findex) {
            ppctl->findex = dp->findex;
        } else {
            ppctl->findex = 1;
        }
    } else {
        ppctl->findex = 1;
    }
    return play_one_file(ppctl);
}
#endif
