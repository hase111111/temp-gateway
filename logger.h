#pragma once

void start_logger_thread();
void stop_logger_thread();
void logger_push(double time, const float* joint);
