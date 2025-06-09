#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include "chess.cpp"

namespace py = pybind11;

PYBIND11_MODULE(chess_engine, m)
{
    m.doc() = "Simple-Chess AI wrapped with pybind11";

    py::class_<Game>(m, "Game")
        .def(py::init<int>(), py::arg("level") = 2)
        .def("setLevel",    &Game::setLevel)
        .def("legalMoves",  &Game::legalMoves)
        .def("playerMove",  &Game::playerMove)

        // ── HERE: release the GIL while C++ thinks ──
        .def("aiMove",      &Game::aiMove,
              py::call_guard<py::gil_scoped_release>())

        .def("board64",     &Game::board64)
        .def("inCheck",     &Game::inCheck)
        .def("isCheckmate", &Game::isCheckmate)
        .def("isStalemate", &Game::isStalemate)
        .def_readonly("lastMove",  &Game::lastMove)
        .def_readonly("whiteCaps", &Game::whiteCaps)
        .def_readonly("blackCaps", &Game::blackCaps);
}
