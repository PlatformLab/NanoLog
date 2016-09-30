/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   LogCompressor.h
 * Author: syang0
 *
 * Created on September 30, 2016, 2:21 AM
 */

#ifndef LOGCOMPRESSOR_H
#define LOGCOMPRESSOR_H

namespace PerfUtils {
class LogCompressor {
public:
    LogCompressor();

    
    void sync();
    void exit();
    void printStats();

    ~LogCompressor();

private:

};
}

#endif /* LOGCOMPRESSOR_H */

