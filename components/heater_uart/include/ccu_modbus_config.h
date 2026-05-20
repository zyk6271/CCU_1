/**
 * @file ccu_modbus_config.h
 * @brief Modbus 设备配置宏定义 (集中管理所有默认参数)
 *
 * 修改此文件可快速适配不同现场, 无需改业务代码.
 *
 * 上报路径:
 *   TCP(TG服务器): AES 加密 → wifi_uart_write_frame
 *   MQTT:          明文 JSON → esp_mqtt_client_publish (不加密)
 */
#ifndef CCU_MODBUS_CONFIG_H_
#define CCU_MODBUS_CONFIG_H_

/* ============================================================ */
/* RS485 串口硬件                                                  */
/* ============================================================ */
#define MB_CFG_TXD_PIN              (21)
#define MB_CFG_RXD_PIN              (20)
#define MB_CFG_RE_PIN               (10)
#define MB_CFG_PORT_NUM             (0)         /* UART_NUM_0 */
#define MB_CFG_BAUD_RATE            (9600)
#define MB_CFG_RESPONSE_TOUT_MS     (1000)

/* ============================================================ */
/* 设备数量                                                       */
/* ============================================================ */
#define MODBUS_DEVICE_TYPE_COUNT    9           /* 主数据设备类型 */
#define MODBUS_CID_TOTAL_COUNT     11          /* 含 TIME 同步 CID */

/* ============================================================ */
/* 从机地址 (Slave Address)                                       */
/* ============================================================ */
#define MB_DEF_ADDR_GAS_FLOW            0x01
#define MB_DEF_ADDR_FRIDGE              0x02
#define MB_DEF_ADDR_CURRENTWATCH        0x03
#define MB_DEF_ADDR_DRYCONTACT          0x04
#define MB_DEF_ADDR_DISHWASHER          0x08
#define MB_DEF_ADDR_GAS_CONC            0x09
#define MB_DEF_ADDR_ULTRA_SONIC         0x0A
#define MB_DEF_ADDR_GAS_STEAMER         0x19
#define MB_DEF_ADDR_GAS_WOK             0x1A

/* ============================================================ */
/* 默认寄存器起始地址                                               */
/* ============================================================ */
#define MB_DEF_REG_GAS_FLOW_START           0x0001
#define MB_DEF_REG_GAS_FLOW_TIME_START      0x0016
#define MB_DEF_REG_FRIDGE_START             0x0000
#define MB_DEF_REG_CURRENTWATCH_START       0x0020
#define MB_DEF_REG_DRYCONTACT_START         0x0000
#define MB_DEF_REG_DISHWASHER_START         0x1320
#define MB_DEF_REG_GAS_CONC_START           0x0016
#define MB_DEF_REG_ULTRA_SONIC_START        0x0000
#define MB_DEF_REG_ULTRA_SONIC_TIME_START   0x0009
#define MB_DEF_REG_GAS_STEAMER_START        0x0000
#define MB_DEF_REG_GAS_WOK_START            0x0000

/* ============================================================ */
/* 默认寄存器数量 (16位寄存器个数)                                   */
/* ============================================================ */
#define MB_DEF_REG_GAS_FLOW_COUNT           13
#define MB_DEF_REG_GAS_FLOW_TIME_COUNT      4
#define MB_DEF_REG_FRIDGE_COUNT             21
#define MB_DEF_REG_CURRENTWATCH_COUNT       6
#define MB_DEF_REG_DRYCONTACT_COUNT         3
#define MB_DEF_REG_DISHWASHER_COUNT         42
#define MB_DEF_REG_GAS_CONC_COUNT           10
#define MB_DEF_REG_ULTRA_SONIC_COUNT        8
#define MB_DEF_REG_ULTRA_SONIC_TIME_COUNT   3
#define MB_DEF_REG_GAS_STEAMER_COUNT        5
#define MB_DEF_REG_GAS_WOK_COUNT            3

/* ============================================================ */
/* 默认通讯使能开关 (1=启用 0=禁用)                                  */
/* 首次上电 / 恢复出厂后使用, 之后以 NVS 为准                        */
/* 默认仅开启燃气流量计, 其他设备按需通过 CMD 0x21 启用              */
/* ============================================================ */
#define MB_DEF_EN_GAS_FLOW              1
#define MB_DEF_EN_FRIDGE                0
#define MB_DEF_EN_CURRENTWATCH          0
#define MB_DEF_EN_DRYCONTACT            0
#define MB_DEF_EN_DISHWASHER            0
#define MB_DEF_EN_GAS_CONC              0
#define MB_DEF_EN_ULTRA_SONIC           0
#define MB_DEF_EN_GAS_STEAMER           0
#define MB_DEF_EN_GAS_WOK               0

/* ============================================================ */
/* 轮询节奏                                                       */
/* ============================================================ */
#define MB_CFG_POLL_INTERVAL_MS         2000
#define MB_CFG_POLL_DEVICE_DELAY_MS     300

#endif /* CCU_MODBUS_CONFIG_H_ */
