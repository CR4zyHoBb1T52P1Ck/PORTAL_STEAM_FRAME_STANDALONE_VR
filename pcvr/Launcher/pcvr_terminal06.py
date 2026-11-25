"""Aperture test-terminal presentation. Importing starts no UI or apps."""
import math,webbrowser
from pcvr_diagnostics import LINKS
BG='#101414';PANEL='#191f20';INK='#e7e4d7';MUTED='#9caaa7';CYAN='#79d6e5';ORANGE='#eea55f'
def colour(value):
 value=max(0,min(100,int(value)));v=12+int(value*.08)
 return '#%02x%02x%02x'%(v,v+4,v+4)
def open_link(label):
 if label not in LINKS:raise ValueError('Unknown community link')
 return webbrowser.open(LINKS[label],new=2)

def edit_actions(widget,kind='text',readonly=False):
 """Native editing/selection with explicit readonly behavior and context menu."""
 import tkinter as tk
 def value(all_text=False):
  try:
   if kind=='entry':return widget.get() if all_text else widget.get()[widget.index('sel.first'):widget.index('sel.last')]
   if kind=='listbox':return '\n'.join(widget.get(i) for i in (range(widget.size()) if all_text else widget.curselection()))
   return widget.get('1.0','end-1c') if all_text else widget.get('sel.first','sel.last')
  except tk.TclError:return ''
 def copy(all_text=False):
  text=value(all_text)
  if text:widget.clipboard_clear();widget.clipboard_append(text)
  return 'break'
 def select_all(_=None):
  if kind=='entry':widget.selection_range(0,'end');widget.icursor('end')
  elif kind=='listbox':widget.selection_set(0,'end')
  else:widget.tag_add('sel','1.0','end-1c');widget.mark_set('insert','1.0')
  widget.focus_set();return 'break'
 menu=tk.Menu(widget,tearoff=False,bg=PANEL,fg=INK,activebackground=CYAN,activeforeground=BG,font=('Consolas',10))
 if not readonly and kind!='listbox':menu.add_command(label='Cut                  Ctrl+X',command=lambda:widget.event_generate('<<Cut>>'))
 menu.add_command(label='Copy                 Ctrl+C',command=copy);menu.add_command(label='Copy all',command=lambda:copy(True))
 if not readonly and kind!='listbox':menu.add_command(label='Paste                Ctrl+V',command=lambda:widget.event_generate('<<Paste>>'))
 menu.add_separator();menu.add_command(label='Select all           Ctrl+A',command=select_all)
 def popup(event):
  widget.focus_set()
  try:menu.tk_popup(event.x_root,event.y_root)
  finally:menu.grab_release()
  return 'break'
 widget.bind('<Button-3>',popup)
 for key in ('<Control-a>','<Control-A>'):widget.bind(key,select_all)
 for key in ('<Control-c>','<Control-C>'):widget.bind(key,lambda _:copy())
 if readonly:
  for key in ('<<Paste>>','<<Cut>>','<Control-v>','<Control-x>'):widget.bind(key,lambda _:'break')
 widget._edit_actions=dict(copy=copy,copy_all=lambda:copy(True),select_all=select_all,value=value,menu=menu)
 return widget._edit_actions

def ancestor_page(widget):
 while widget is not None:
  page=getattr(widget,'_pcvr_page',None)
  if page:return page
  widget=getattr(widget,'master',None)

class ScrollPage:
 def __init__(self,notebook,title):
  import tkinter as tk
  from tkinter import ttk
  self.outer=ttk.Frame(notebook);self.outer.columnconfigure(0,weight=1);self.outer.rowconfigure(0,weight=1)
  self.canvas=tk.Canvas(self.outer,bg=PANEL,highlightthickness=0,takefocus=True,yscrollincrement=20)
  self.bar=ttk.Scrollbar(self.outer,orient='vertical',command=self.canvas.yview);self.canvas.configure(yscrollcommand=self.bar.set)
  self.canvas.grid(row=0,column=0,sticky='nsew');self.bar.grid(row=0,column=1,sticky='ns')
  self.body=ttk.Frame(self.canvas,padding=(24,18));self.body._pcvr_page=self;self.outer._pcvr_page=self
  self.item=self.canvas.create_window(0,0,window=self.body,anchor='nw')
  self.body.bind('<Configure>',lambda _:self.canvas.configure(scrollregion=self.canvas.bbox('all')))
  self.canvas.bind('<Configure>',lambda e:self.canvas.itemconfigure(self.item,width=e.width))
  self.canvas.bind('<Prior>',lambda _:self.move(-1,'pages'));self.canvas.bind('<Next>',lambda _:self.move(1,'pages'))
  self.canvas.bind('<Home>',lambda _:self.top());self.canvas.bind('<End>',lambda _:self.bottom())
  notebook.add(self.outer,text=title);window=notebook.winfo_toplevel()
  if not getattr(window,'_pcvr_wheel',False):
   window._pcvr_wheel=True
   def route(event):
    if event.widget.winfo_class() in ('Text','Listbox','TEntry','Entry','TScale'):return
    page=ancestor_page(event.widget)
    if page:return page.wheel(event)
   window.bind('<MouseWheel>',route,add='+')
 def move(self,amount,kind='units'):self.canvas.yview_scroll(amount,kind);return 'break'
 def wheel(self,event):
  delta=getattr(event,'delta',0)
  return self.move((-1 if delta>0 else 1)*max(1,int(abs(delta)/120))*3) if delta else 'break'
 def top(self):self.canvas.yview_moveto(0);return 'break'
 def bottom(self):self.canvas.yview_moveto(1);return 'break'
def page(notebook,title):return ScrollPage(notebook,title).body

def note(parent,text,muted=False):
 import tkinter as tk
 widget=tk.Text(parent,height=2,width=1,wrap='word',font=('Consolas',10),background=PANEL,foreground=MUTED if muted else INK,
  selectbackground=CYAN,selectforeground=BG,relief='flat',borderwidth=0,highlightthickness=0,padx=0,pady=4,takefocus=True,cursor='xterm',exportselection=False)
 widget._pcvr_note=True;widget._pcvr_muted=muted
 widget.insert('1.0',text);widget.configure(state='disabled');widget.pack(fill='x',pady=5);edit_actions(widget,readonly=True)
 pending=[False]
 def fit(_=None):
  if pending[0]:return
  pending[0]=True
  def update():
   pending[0]=False
   if not widget.winfo_exists():return
   lines=widget.count('1.0','end-1c','displaylines');height=max(1,(lines[0] if lines else 0)+1)
   if int(widget.cget('height'))!=height:widget.configure(height=height)
  widget.after_idle(update)
 widget.bind('<Configure>',fit)
 widget.bind('<MouseWheel>',lambda e:ancestor_page(widget).wheel(e) if ancestor_page(widget) else 'break')
 return widget

class Console:
 def __init__(self,parent):
  import tkinter as tk
  from tkinter import ttk
  self.frame=ttk.Frame(parent,padding=(16,10));self.frame.columnconfigure(0,weight=1);self.frame.rowconfigure(1,weight=1)
  bar=ttk.Frame(self.frame);bar.grid(row=0,column=0,columnspan=2,sticky='ew',pady=(0,7))
  ttk.Label(bar,text='SESSION TELEMETRY',style='Caption.TLabel').pack(side='left');self.follow=tk.BooleanVar(value=True)
  ttk.Checkbutton(bar,text='Follow',variable=self.follow).pack(side='right',padx=8)
  self.text=tk.Text(self.frame,height=5,width=1,wrap='none',state='disabled',bg='#0b1011',fg=INK,font=('Consolas',10),selectbackground=CYAN,selectforeground=BG,insertbackground=CYAN,exportselection=False,highlightthickness=1,highlightbackground='#354345',highlightcolor=CYAN,padx=10,pady=8)
  self.text.grid(row=1,column=0,sticky='nsew');ys=ttk.Scrollbar(self.frame,command=self.text.yview);ys.grid(row=1,column=1,sticky='ns')
  xs=ttk.Scrollbar(self.frame,orient='horizontal',command=self.text.xview);xs.grid(row=2,column=0,sticky='ew');self.text.configure(yscrollcommand=ys.set,xscrollcommand=xs.set)
  self.actions=edit_actions(self.text,readonly=True)
  for tag,color in (('warning',ORANGE),('success',CYAN)):self.text.tag_configure(tag,foreground=color)
  self.text.tag_configure('found',background=ORANGE,foreground=BG)
  controls=ttk.Frame(self.frame);controls.grid(row=3,column=0,columnspan=2,sticky='ew',pady=(7,0))
  for label,fn in [('Copy selection',self.actions['copy']),('Copy all',self.actions['copy_all']),('Select all',self.actions['select_all']),('Latest',lambda:self.text.see('end'))]:ttk.Button(controls,text=label,style='Small.TButton',command=fn).pack(side='left',padx=(0,5))
  self.query=tk.StringVar();self.search=ttk.Entry(controls,textvariable=self.query,width=16);self.search.pack(side='left',fill='x',expand=True,padx=(10,5));edit_actions(self.search,'entry')
  ttk.Button(controls,text='Find next',style='Small.TButton',command=self.find).pack(side='left')
  self.search.bind('<Return>',lambda _:self.find());self.text.bind('<Control-f>',lambda _:self.focus_search())
 def focus_search(self):self.search.focus_set();self.search.selection_range(0,'end');return 'break'
 def append(self,value):
  end=self.text.yview()[1]>=.985;selected=bool(self.text.tag_ranges('sel'));value=str(value)
  tag='warning' if value.startswith('Action failed:') else 'success' if value.startswith(('Portal started','Portal exited: normal')) else ''
  self.text.configure(state='normal');self.text.insert('end',value+'\n',tag);self.text.configure(state='disabled')
  if self.follow.get() and end and not selected:self.text.see('end')
 def find(self):
  query=self.query.get();self.text.tag_remove('found','1.0','end')
  if not query:return 'break'
  start=self.text.index('insert+1c');at=self.text.search(query,start,stopindex='end',nocase=True)
  if not at:at=self.text.search(query,'1.0',stopindex=start,nocase=True)
  if at:self.text.tag_add('found',at,at+'+%dc'%len(query));self.text.mark_set('insert',at);self.text.see(at)
  return 'break'

def setup(window,tk,ttk,root):
 from pcvr_appearance07 import setup as display_setup
 return display_setup(window,tk,ttk,root)

def iris(parent):
 import tkinter as tk
 c=tk.Canvas(parent,width=48,height=48,bg=PANEL,highlightthickness=0);c._pcvr_iris=True
 for i in range(8):
  a=i*math.pi/4;points=[]
  for radius,angle in ((21,a),(21,a+.63),(9,a+.9),(9,a+.2)):points.extend((24+radius*math.cos(angle),24+radius*math.sin(angle)))
  c.create_polygon(*points,fill=INK if i%2 else CYAN,outline=PANEL,tags=('iris_odd' if i%2 else 'iris_even',))
 return c
