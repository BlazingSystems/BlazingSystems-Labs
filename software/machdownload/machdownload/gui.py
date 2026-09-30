import os, threading, time
from pathlib import Path
import tkinter as tk
from tkinter import ttk, filedialog, messagebox
from .core import DownloadJob, DownloadError, Cancelled, ResourceChanged

class App(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("MachDownload 0.1")
        self.geometry("760x390")
        self.minsize(650,350)
        self.stop_event=threading.Event()
        self.url=tk.StringVar()
        self.dest=tk.StringVar(value=str(Path.home()/"Downloads"))
        self.connections=tk.IntVar(value=8)
        self.status=tk.StringVar(value="Ready")
        self.speed=tk.StringVar(value="")
        self._build()

    def _build(self):
        f=ttk.Frame(self,padding=14); f.pack(fill="both",expand=True)
        ttk.Label(f,text="MachDownload",font=("Segoe UI",18,"bold")).grid(row=0,column=0,columnspan=3,sticky="w",pady=(0,12))
        ttk.Label(f,text="URL").grid(row=1,column=0,sticky="w")
        ttk.Entry(f,textvariable=self.url).grid(row=2,column=0,columnspan=3,sticky="ew",pady=(3,10))
        ttk.Label(f,text="Save to").grid(row=3,column=0,sticky="w")
        ttk.Entry(f,textvariable=self.dest).grid(row=4,column=0,columnspan=2,sticky="ew",pady=(3,10))
        ttk.Button(f,text="Browse",command=self.browse).grid(row=4,column=2,padx=(8,0))
        ttk.Label(f,text="Connections").grid(row=5,column=0,sticky="w")
        ttk.Spinbox(f,from_=1,to=32,textvariable=self.connections,width=8).grid(row=5,column=1,sticky="w")
        self.pb=ttk.Progressbar(f,mode="determinate"); self.pb.grid(row=6,column=0,columnspan=3,sticky="ew",pady=(18,4))
        ttk.Label(f,textvariable=self.status).grid(row=7,column=0,columnspan=2,sticky="w")
        ttk.Label(f,textvariable=self.speed).grid(row=7,column=2,sticky="e")
        b=ttk.Frame(f); b.grid(row=8,column=0,columnspan=3,sticky="ew",pady=(20,0))
        self.go=ttk.Button(b,text="Download",command=self.start); self.go.pack(side="left")
        ttk.Button(b,text="Stop",command=self.stop).pack(side="left",padx=8)
        ttk.Button(b,text="Open Folder",command=self.open_folder).pack(side="right")
        f.columnconfigure(0,weight=1); f.columnconfigure(1,weight=1)

    def browse(self):
        p=filedialog.askdirectory(initialdir=self.dest.get())
        if p:self.dest.set(p)
    def open_folder(self):
        p=self.dest.get()
        try: os.startfile(p)
        except Exception: pass
    def stop(self):
        self.stop_event.set(); self.status.set("Stopping safely…")
    def progress(self,done,total,speed):
        def u():
            if total:
                self.pb["maximum"]=total; self.pb["value"]=done
                self.status.set(f"{done/1048576:.1f} / {total/1048576:.1f} MiB")
            else:
                self.pb.configure(mode="indeterminate"); self.pb.start(30)
                self.status.set(f"{done/1048576:.1f} MiB")
            self.speed.set(f"{speed/1048576:.2f} MiB/s")
        self.after(0,u)
    def start(self):
        if not self.url.get().strip(): return
        self.stop_event=threading.Event(); self.go.state(["disabled"])
        self.status.set("Probing server…"); self.pb["value"]=0
        def worker():
            try:
                p=DownloadJob(self.url.get().strip(),self.dest.get(),self.connections.get(),
                              progress=self.progress,stop_event=self.stop_event).run()
                self.after(0,lambda: (self.status.set(f"Complete: {p.name}"), self.speed.set(""),
                                      messagebox.showinfo("MachDownload","Download completed successfully.")))
            except Cancelled:
                self.after(0,lambda:self.status.set("Stopped. Partial data kept for resume."))
            except Exception as e:
                self.after(0,lambda e=e:(self.status.set("Error"),messagebox.showerror("MachDownload",str(e))))
            finally:self.after(0,lambda:self.go.state(["!disabled"]))
        threading.Thread(target=worker,daemon=True).start()

def main(): App().mainloop()
if __name__=="__main__":main()
