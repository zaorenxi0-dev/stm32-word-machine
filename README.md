# 嵌入式单词机（STM32F407 + LVGL + FreeRTOS）

基于 **STM32F407ZGT6** 的桌面级多功能学习终端：在 320×240 触摸屏上以 LVGL 图形界面提供**单词背诵**、**WAV 音乐播放**、**科学计算器**三大功能，底层由 FreeRTOS 调度，数据与音频资源存放于 SD 卡。

> 硬件平台：正点原子 探索者 STM32F407 开发板（atk_f407）

---

## 一、功能模块

| 模块 | 界面 | 说明 |
|---|---|---|
| **单词机** | `screen` | 从 SD 卡 `0:/text/word.csv` 读取词库，屏幕显示单词与中文释义，按钮切换上/下一个词；支持词库分页索引 |
| **音乐播放器** | `screen_2` / 动态构建 | 扫描 `0:/MUSIC` 目录下的 16bit PCM WAV，支持播放/暂停、上/下一曲、音量调节，播放进度条实时刷新 |
| **计算器** | `screen_1` | 20 键按钮矩阵，支持四则运算、括号、小数与一元正负号，表达式求值结果自实现格式化输出 |

界面由 **GUI Guider** 生成（`screen` / `screen_1` / `screen_2` 三个屏），业务界面（播放器）另按 320×240 手工布局。

## 二、技术要点

**1. LVGL + FreeRTOS 多任务架构**

`main.c` 完成外设初始化后交由 `lvgl_demo()` 启动 FreeRTOS，创建三级任务：

| 任务 | 优先级 | 职责 |
|---|---|---|
| `start_task` | 1 | 临界区内创建其余任务后自删除 |
| `lv_demo_task` | 3 | `lv_timer_handler()` 周期驱动 LVGL 刷新（5ms） |
| `led_task` | 4 | LED0 心跳指示 |

界面任务与硬件操作严格分离：UI 只读取播放器状态、只投递「请求」，真正接触 FatFS / WM8978 / I2S / DMA 的只有 `audio_player` 任务，从架构上规避了对 SDIO 与 DMA 的并发访问。

**2. I2S + DMA 双缓冲非阻塞音频**

音频播放沿用 I2S2(SPI2) + DMA1_Stream4 的**双缓冲（Double Buffer）**机制：两块等长缓冲区交 DMA 自动往返搬运，每搬完一块触发一次传输完成中断，中断里只记录当前目标缓冲并置标志，音频任务收到标志后填充刚播完的空闲缓冲 —— 读文件与放音完全并行，全程无需阻塞式延时。

**3. FatFS + 外部 SRAM 词库索引**

词库为纯文本 CSV，逐行 `单词,释义`。开机时顺序扫描一次文件建立**偏移索引表**（每条记录的字节偏移），索引写入外部 SRAM（按 1000 条分块，每块 4000 字节），随后按 8 词一页重建分页词表。运行时按索引直接 `f_lseek` 定位，无需重复扫描文本，响应明显快于逐行查找。

**4. 双栈法表达式求值**

计算器不依赖任何表达式解析库：运算符栈 + 操作数栈扫描中缀表达式，栈顶优先级 ≥ 当前运算符时立即出栈计算，从而保证乘除先于加减、同级从左到右；一元正负号统一改写为 `0 - 操作数` 处理，无需为符号单独增加分支。结果字符串化自行实现，不依赖 Keil MicroLIB 下的 `%f`。

## 三、目录结构

```
├── Drivers/
│   ├── BSP/                  板级驱动：LCD / TOUCH / SDIO / SRAM / WM8978 / IIS /
│   │   └── WORD/             ★ 单词机词库索引与分页模块
│   ├── CMSIS/                Cortex-M4 内核与器件头文件
│   ├── STM32F4xx_HAL_Driver/ ST HAL 库
│   └── SYSTEM/               时钟 / 延时 / 串口
├── Middlewares/
│   ├── FATFS/                FatFS 文件系统（含 exfuns 扩展）
│   ├── FreeRTOS/             实时内核
│   ├── LVGL/                 LVGL 图形库 + GUI Guider 生成的界面工程
│   │   └── GUI_APP/          生成界面（generated/）、字体、图片、入口 lv_mainstart.c
│   └── MALLOC/               内部/外部 SRAM 内存池管理
├── Projects/MDK-ARM/         Keil MDK 工程（atk_f407.uvprojx）
└── User/
    ├── APP/                  ★ 业务代码
    │   ├── player.c/.h       WAV 播放器：DMA 双缓冲 + 任务解耦
    │   ├── player_ui.c/.h    播放器界面（320×240 手工布局）
    │   ├── calc.c/.h         计算器核心：双栈表达式求值
    │   ├── calc_ui.c/.h      计算器界面
    │   └── audioplay.c/.h    WAV 文件头解析（RIFF / FMT / fact 块）
    ├── main.c                主程序：外设初始化 → 词库索引 → 启动 RTOS
    ├── lvgl_demo.c/.h        LVGL 与 FreeRTOS 任务创建入口
    ├── FreeRTOSConfig.h      RTOS 配置
    └── stm32f4xx_it.c        中断服务
```

（★ 为自己编写的模块）

## 四、编译与烧录

1. 用 **Keil MDK5** 打开 `Projects/MDK-ARM/atk_f407.uvprojx`
2. 编译器为 **ARM Compiler 6（AC6）**
3. **重要**：`Edit → Configuration → Encoding` 选择 **UTF-8**，源码为 UTF-8 编码
4. Rebuild 后烧录，或直接使用编译产物 `Output/atk_f407.hex`

命令行编译：

```bat
UV4.exe -b Projects\MDK-ARM\atk_f407.uvprojx -o build.log
```

> `Output/` 未纳入版本库，重新编译会自动生成。

## 五、使用说明

**SD 卡目录结构**

```
SD 卡根目录
├── MUSIC/            存放 16bit PCM 的 .wav 文件
└── text/
    └── word.csv      词库，每行一条：单词,释义
```

`word.csv` 示例：

```
abandon,放弃；抛弃
benefit,利益；受益
capable,有能力的
```

上电后程序自动完成：挂载 SD 卡 → 扫描词库建立索引 → 生成分页词表 → 进入 LVGL 图形界面。

**串口**：USART1，115200-8-N-1，可查看启动与运行日志。

## 六、已知限制

- 音频仅支持 16bit PCM WAV，采样率需落在 I2S 分频表支持范围内（8k / 16k / 22.05k / 32k / 44.1k / 48k）；24bit / 32bit / ADPCM / MP3 伪装扩展名会返回 `PLAYER_ERR_FORMAT` 并提示，不做不可靠的静默转换
- 词库索引按每块 1000 条分块写入外部 SRAM，词条总量与外部 SRAM 容量相关
- 本项目基于正点原子官方例程开发（HAL 库、BSP 驱动、LVGL 移植层、FatFS 与 FreeRTOS 移植来自厂商示例），业务逻辑与界面为自行实现
