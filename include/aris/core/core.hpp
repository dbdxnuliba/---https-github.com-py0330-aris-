﻿#ifndef ARIS_CORE_H_
#define ARIS_CORE_H_

#include <aris/core/basic_type.hpp>
#include <aris/core/object.hpp>
#include <aris/core/msg.hpp>
#include <aris/core/log.hpp>
#include <aris/core/expression_calculator.hpp>
#include <aris/core/socket.hpp>
#include <aris/core/pipe.hpp>
#include <aris/core/command.hpp>

#endif // ARIS_H_

// to do list 2019-04-14:
// - improve socket on linux                                              check
// - check motion enable state
// - remove margin(0.005) of manual plan
// - generate 6-axis from DH                                              check
// - fix moveAbsolute2 bug on linux                                       check
// - check control pdos config successfully and throw exception           half check
// - make better registration                                             check
// - find bugs of Qianchao                                                check
// - make dynamic calibration as a plan                                   


// to do list 2019-04-21:
// - use extern template to reduce compile time


// to do list 2019-05-07:
// - svd decomposition
// - check ethercat link
// - scan slave info to motion
// - generate ur from DH
// - 7-axis inverse solver                                                check
// - generate stewart from fix points
// - judge mechanism is 7-axis 6-axis ur stewart
// - use pre-compiled header(stdafx)
// - exclusive plan option
// - check scan result when controller start

// to do list 2019-05-14:
// - make pdo of curent/targetpos/... can be customized



