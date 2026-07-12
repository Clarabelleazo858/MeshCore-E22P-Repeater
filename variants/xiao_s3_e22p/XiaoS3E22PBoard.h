#pragma once

#include <Arduino.h>
#include <helpers/ESP32Board.h>

class XiaoS3E22PBoard : public ESP32Board {
public:
  XiaoS3E22PBoard() { }

  const char* getManufacturerName() const override {
    return "Xiao S3 E22P";
  }
};
