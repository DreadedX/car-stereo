from binascii import unhexlify
import tkinter.ttk as ttk
import tkinter as tk
import serial

APPNAME = "CAN Bus Spy"
BLOCK_WIDTH = 8
BLOCK_HEIGHT = 32

class MainWindow:
    def __init__(self, parent):
        self.parent = parent
        self.create_widgets()
        self.create_layout()

    def create_widgets(self):
        self.frame = ttk.Frame(self.parent)

        self.viewText = tk.Text(self.frame, height=BLOCK_HEIGHT, width=7 + (BLOCK_WIDTH * 4))
        self.viewText.tag_configure("ascii", foreground="green")
        self.viewText.tag_configure("error", foreground="red")
        self.viewText.tag_configure("hexspace", foreground="navy")
        self.viewText.tag_configure("graybg", background="lightgray")

        self.viewText.tag_configure("id", foreground="blue")

    def create_layout(self):
        self.viewText.grid(row=1, column=0, columnspan=6, sticky=tk.NSEW)
        self.frame.grid(row=0, column=0, sticky=tk.NSEW)

    def show_data(self, table):
        self.viewText.delete("1.0", "end")

        for id, data in sorted(table.items()):
            # Print the frame id
            self.viewText.insert("end", "{:04d}".format(id), ("id"))
            self.viewText.insert("end", "  ")

            # Print the frame as hex
            for byte in data:
                tags = ()
                if 0x20 < byte < 0x7F:
                    tags = ("ascii")
                self.viewText.insert("end", "{:02X} ".format(byte), tags)

            # Insert some padding
            self.viewText.insert("end", " " * (BLOCK_WIDTH - len(data)) * 3)

            self.viewText.insert("end", " ")

            # Print the frame as ascii
            for char in data.decode("ascii", errors="replace"):
                tags = ()

                if char in "\u2028\u2029\t\n\r\v\f\uFFFD":
                    char = "."
                    tags = ("graybg" if char == "\uFFFD" else "error",)
                elif 0x20 < ord(char) < 0x7F:
                    tags = ("ascii",)
                elif not 0x20 <= ord(char) <= 0xFFFF: # Tcl/Tk limit
                    char = "?"
                    tags = ("error",)

                self.viewText.insert("end", char, tags)

            self.viewText.insert("end", "\n")

    def quit(self, event=None):
        self.parent.destroy()

class InvalidFrame(Exception):
    pass

def parse(line):
    # FRAME ID=234 LEN=3 AB:EE:69
    frame = line.split(':', maxsplit=3)

    try:
        frame_id = int(frame[1][3:])
        frame_length = int(frame[2][4:])

        hex_data = frame[3].replace(':', '')

        data = unhexlify(hex_data)

    except (IndexError, ValueError) as exc:
        raise InvalidFrame("Invalid frame {}".format(line)) from exc

    if len(data) != frame_length:
        raise InvalidFrame("Wrong frame length or invalid data: {}".format(line))

    return frame_id, data

ser = serial.Serial("/dev/ttyUSB0", 115200)
table = {}

def task():
    if ser.in_waiting:
        line = str(ser.readline().rstrip(), "ascii")
        print(line)
        id, data = parse(line)
        table[id] = data

    window.show_data(table)
    app.after(10, task)


app = tk.Tk()
app.title(APPNAME)
window = MainWindow(app)
app.protocol("WM_DELETE_WINDOW", window.quit)
app.resizable(width=False, height=False)
app.after(0, task)
app.mainloop()
