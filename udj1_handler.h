#pragma once

// UDJ1 UDPパケットを受信して、各ジョイント角度を CAN に送信する．
// SystemStateが RUN の間だけ処理を行う．
void start_udj1_thread();
void stop_udj1_thread();
