//
// Created by llogan on 7/25/24.
//

#ifdef CHIMAERA_ENABLE_PYTHON

#include "chimaera/monitor/python_wrapper.h"
#include "chimaera/monitor/least_squares.h"


PYBIND11_MODULE(chimaera_monitor, m) {
  // Create python bindings for LeastSquares
    py::class_<chi::LeastSquares>(m, "LeastSquares")
        .def(py::init<>())
        .def("add", &chi::LeastSquares::Add);
}

#endif
