
#pragma once

enum class SystemState : int {
    INIT,  // Odriveのキャリブレーション命令中.
    CALIBRATED,  // キャリブレーション完了，クローズドループ制御に入る．
    READY,
    RUN
};
