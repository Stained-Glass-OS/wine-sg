#!/usr/bin/python3
# xdndqt-app.py LOG: a Qt program shaped like Kdenlive for test/xdndqt-gate.sh --
# a "bin" list whose items drag with a private type (as Kdenlive's
# kdenlive/producerslist) and a "timeline" that takes that drag (Qt's own drag
# inside one program) and files (text/uri-list, from File Explorer). Logs:
#   READY <top-level X window id>
#   DROP internal <item>     the bin's item reached the timeline
#   DROP uri <urls>          files reached the timeline
import sys
from PyQt6.QtCore import Qt, QMimeData
from PyQt6.QtGui import QDrag
from PyQt6.QtWidgets import QApplication, QWidget, QListWidget, QLabel, QHBoxLayout

log = open(sys.argv[1], "a", buffering=1)
MIME = "application/x-sg-producerslist"


class Bin(QListWidget):
    def startDrag(self, actions):
        it = self.currentItem()
        if not it:
            return
        md = QMimeData()
        md.setData(MIME, it.text().encode())
        d = QDrag(self)
        d.setMimeData(md)
        log.write("DRAGSTART %s\n" % it.text())
        r = d.exec(Qt.DropAction.CopyAction | Qt.DropAction.MoveAction)
        log.write("DRAGRESULT %d\n" % r.value)


class Timeline(QLabel):
    def __init__(self):
        super().__init__("timeline")
        self.setAcceptDrops(True)
        self.setStyleSheet("background:#203040;color:white")
        self.setAlignment(Qt.AlignmentFlag.AlignCenter)

    def dragEnterEvent(self, e):
        md = e.mimeData()
        if md.hasFormat(MIME) or md.hasUrls():
            e.acceptProposedAction()

    def dragMoveEvent(self, e):
        e.acceptProposedAction()

    def dropEvent(self, e):
        md = e.mimeData()
        if md.hasFormat(MIME):
            log.write("DROP internal %s\n" % bytes(md.data(MIME)).decode())
        elif md.hasUrls():
            log.write("DROP uri %s\n" % " ".join(u.toString() for u in md.urls()))
        e.acceptProposedAction()


app = QApplication(sys.argv[:1])
w = QWidget()
w.setWindowTitle("Qt Timeline")
w.setGeometry(60, 60, 640, 300)
lay = QHBoxLayout(w)
b = Bin()
b.addItems(["clip-a", "clip-b"])
b.setDragEnabled(True)
b.setFixedWidth(180)
lay.addWidget(b)
lay.addWidget(Timeline(), 1)
w.show()
app.processEvents()
log.write("READY %d\n" % int(w.winId()))
app.exec()
