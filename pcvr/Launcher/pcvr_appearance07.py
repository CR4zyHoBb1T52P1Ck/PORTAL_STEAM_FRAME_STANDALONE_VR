"""Saved launcher display preferences and per-window palettes; no import-time UI."""
from pathlib import Path
import json,os,tempfile,time

DEFAULTS=dict(mode='dark',accent='blue',text_size='comfortable',grid=True,dimmer=65,pane_ratio=.68)
MODES={'Dark':'dark','Light':'light'}
ACCENTS={'Portal blue':'blue','Portal orange':'orange','Terminal green':'green'}
SIZES={'Compact':'compact','Comfortable':'comfortable','Large':'large'}
def normalize(raw):
 result=dict(raw) if isinstance(raw,dict) else {}
 for key,choices in (('mode',MODES.values()),('accent',ACCENTS.values()),('text_size',SIZES.values())):
  if result.get(key) not in choices:result[key]=DEFAULTS[key]
 result['grid']=result.get('grid') if isinstance(result.get('grid'),bool) else True
 value=result.get('dimmer');result['dimmer']=max(0,min(100,int(value))) if isinstance(value,(int,float)) and not isinstance(value,bool) and abs(value)<1e6 else 65
 ratio=result.get('pane_ratio');result['pane_ratio']=max(.45,min(.82,ratio)) if isinstance(ratio,(int,float)) and not isinstance(ratio,bool) and .01<ratio<1 else .68
 return result
def load(path):
 try:
  path=Path(path)
  if path.stat().st_size>65536:return normalize({})
  return normalize(json.loads(path.read_text(encoding='utf-8')))
 except (OSError,ValueError,UnicodeError):return normalize({})
def save(path,values,backup=False):
 path=Path(path)
 if path.is_symlink():raise ValueError('Linked preference file preserved')
 path.parent.mkdir(parents=True,exist_ok=True)
 if path.is_file() and path.stat().st_size>65536:raise ValueError('Oversized preference file preserved')
 if backup and path.is_file():
  before=path.parent/'Appearance-Before';before.mkdir(exist_ok=True)
  (before/('Appearance-'+str(time.time_ns())+'.json')).write_bytes(path.read_bytes())
 current=load(path);wanted=normalize(values)
 for key in DEFAULTS:current[key]=wanted[key]
 fd,temp=tempfile.mkstemp(prefix='.appearance-',dir=path.parent)
 try:
  with os.fdopen(fd,'w',encoding='utf-8') as file:json.dump(current,file,indent=2);file.write('\n');file.flush();os.fsync(file.fileno())
  os.replace(temp,path)
 finally:
  if os.path.exists(temp):os.unlink(temp)
 return current
def mix(a,b,amount):
 return '#'+''.join('%02x'%round(int(a[i:i+2],16)*(1-amount)+int(b[i:i+2],16)*amount) for i in (1,3,5))
def luminance(color):
 rgb=[int(color[i:i+2],16)/255 for i in (1,3,5)]
 return sum(k*(v/12.92 if v<=.04045 else ((v+.055)/1.055)**2.4) for k,v in zip((.2126,.7152,.0722),rgb))
def contrast(a,b):
 a,b=sorted((luminance(a),luminance(b)));return (b+.05)/(a+.05)
def palette(mode='dark',accent='blue'):
 light=mode=='light'
 p=dict(bg='#e9e8df',panel='#f8f7ef',field='#ffffff',ink='#253436',muted='#586768',border='#abb9b6',grid='#d6dbd3',warning='#994422') if light else dict(bg='#101516',panel='#192123',field='#0c1214',ink='#e7e7d9',muted='#a2b3af',border='#425b60',grid='#243439',warning='#f1b278')
 p['accent']=({'blue':'#08647a','orange':'#994b14','green':'#226535'} if light else {'blue':'#79d6e5','orange':'#f1ad68','green':'#9cdaa5'}).get(accent,'#79d6e5')
 if accent=='green':p['ink']='#294e32' if light else '#bce7c4';p['muted']='#4c6951' if light else '#a4b8a7'
 p['on_accent']=max(('#101b1d','#ffffff'),key=lambda x:contrast(x,p['accent']))
 p['button']=mix(p['panel'],p['accent'],.08);p['hover']=mix(p['panel'],p['accent'],.19)
 return p

class Controller:
 def __init__(self,window,tk,ttk,root):
  self.window=window;self.tk=tk;self.ttk=ttk;self.path=Path(root).parent/'State/Launcher-Appearance.json';self.saved=load(self.path)
  self.mode=tk.StringVar(value=next(k for k,v in MODES.items() if v==self.saved['mode']))
  self.accent=tk.StringVar(value=next(k for k,v in ACCENTS.items() if v==self.saved['accent']))
  self.text_size=tk.StringVar(value=next(k for k,v in SIZES.items() if v==self.saved['text_size']))
  self.enabled=tk.BooleanVar(value=self.saved['grid']);self.dimmer=tk.IntVar(value=self.saved['dimmer'])
  self.pane_ratio=self.saved['pane_ratio'];self.pending=None;self.backed=False;self.dirty=False;self.notify=lambda text:None
  self.style=ttk.Style(window);self.style.theme_use('clam')
  self.canvas=tk.Canvas(window,highlightthickness=0);self.canvas._pcvr_background=True;self.canvas.place(x=0,y=0,relwidth=1,relheight=1);self.canvas.tk.call('lower',str(self.canvas))
  for var in (self.mode,self.accent,self.text_size):var.trace_add('write',self.changed)
  for var in (self.enabled,self.dimmer):var.trace_add('write',self.grid_changed)
  window.bind('<Configure>',self.paint,add='+');window.bind('<Destroy>',self.destroyed,add='+')
  self.apply()
 def values(self):
  return normalize(dict(mode=MODES.get(self.mode.get(),'dark'),accent=ACCENTS.get(self.accent.get(),'blue'),text_size=SIZES.get(self.text_size.get(),'comfortable'),grid=self.enabled.get(),dimmer=self.dimmer.get(),pane_ratio=self.pane_ratio))
 def changed(self,*_):
  self.dirty=True
  self.apply()
  if self.pending:self.window.after_cancel(self.pending)
  self.pending=self.window.after(250,self.flush)
 def grid_changed(self,*_):
  self.dirty=True;self.paint()
  if self.pending:self.window.after_cancel(self.pending)
  self.pending=self.window.after(250,self.flush)
 def flush(self):
  if self.pending:self.window.after_cancel(self.pending);self.pending=None
  if not self.dirty:return
  try:self.saved=save(self.path,self.values(),backup=not self.backed);self.backed=True;self.dirty=False
  except (OSError,ValueError) as error:self.notify('Display preference could not be saved: '+str(error))
 def destroyed(self,event):
  if event.widget==self.window and self.pending:
   self.window.after_cancel(self.pending);self.pending=None
 def set_pane(self,ratio):
  self.pane_ratio=max(.45,min(.82,ratio));self.dirty=True
  if self.pending:self.window.after_cancel(self.pending)
  self.pending=self.window.after(250,self.flush)
 def paint(self,event=None):
  if event is not None and event.widget!=self.window:return
  if not self.canvas.winfo_exists():return
  p=palette(self.values()['mode'],self.values()['accent']);w=max(1,self.window.winfo_width());h=max(1,self.window.winfo_height())
  self.canvas.delete('all');self.canvas.configure(bg=p['bg'])
  if self.enabled.get():
   color=mix(p['grid'],p['bg'],self.dimmer.get()/150)
   for x in range(0,w,32):self.canvas.create_line(x,0,x,h,fill=color)
   for y in range(0,h,32):self.canvas.create_line(0,y,w,y,fill=color)
 def apply(self):
  values=self.values();p=palette(values['mode'],values['accent']);self.colors=p
  base={'compact':9,'comfortable':10,'large':12}[values['text_size']];s=self.style
  self.window.configure(bg=p['bg'])
  s.configure('.',font=('Consolas',base),background=p['panel'],foreground=p['ink'])
  s.configure('TFrame',background=p['panel']);s.configure('TLabel',background=p['panel'],foreground=p['ink'])
  s.configure('Title.TLabel',font=('Consolas',base+9,'bold'),foreground=p['ink']);s.configure('Caption.TLabel',font=('Consolas',max(9,base-1),'bold'),foreground=p['accent'])
  s.configure('TButton',font=('Consolas',base),background=p['button'],foreground=p['ink'],padding=(16,8),borderwidth=1,relief='flat',focuscolor=p['accent'],lightcolor=p['border'],darkcolor=p['border'],bordercolor=p['border'])
  s.map('TButton',background=[('pressed',p['hover']),('active',p['hover'])],foreground=[('disabled',p['muted'])],bordercolor=[('focus',p['accent'])])
  s.configure('Small.TButton',font=('Consolas',max(9,base-1)),padding=(9,5))
  s.configure('Play.TButton',font=('Consolas',base+2,'bold'),background=p['accent'],foreground=p['on_accent'],padding=(18,11))
  s.map('Play.TButton',background=[('active',mix(p['accent'],p['on_accent'],.1)),('pressed',p['accent'])],foreground=[('active',p['on_accent']),('pressed',p['on_accent'])])
  s.configure('TNotebook',background=p['bg'],borderwidth=0);s.configure('TNotebook.Tab',padding=(16,9),background=p['button'],foreground=p['muted'],lightcolor=p['border'],bordercolor=p['border'])
  s.map('TNotebook.Tab',background=[('selected',p['panel']),('active',p['hover'])],foreground=[('selected',p['accent']),('active',p['ink'])])
  for kind in ('TEntry','TCombobox'):
   s.configure(kind,font=('Consolas',base),fieldbackground=p['field'],foreground=p['ink'],insertcolor=p['accent'],selectbackground=p['accent'],selectforeground=p['on_accent'],padding=5,lightcolor=p['border'],darkcolor=p['border'],bordercolor=p['border'],arrowcolor=p['accent'])
   s.map(kind,fieldbackground=[('readonly',p['panel'])],foreground=[('readonly',p['ink'])],selectbackground=[('!disabled',p['accent'])],selectforeground=[('!disabled',p['on_accent'])])
  s.configure('Status.TEntry',font=('Consolas',max(9,base-1)),padding=3)
  s.configure('TCheckbutton',background=p['panel'],foreground=p['ink']);s.map('TCheckbutton',background=[('active',p['hover'])])
  s.configure('TLabelframe',background=p['panel'],bordercolor=p['border']);s.configure('TLabelframe.Label',foreground=p['accent'],font=('Consolas',base,'bold'))
  for kind in ('Vertical.TScrollbar','Horizontal.TScrollbar'):s.configure(kind,troughcolor=p['field'],background=p['border'],arrowcolor=p['ink'],lightcolor=p['border'],darkcolor=p['border'],bordercolor=p['border'])
  self.window.option_add('*TCombobox*Listbox.background',p['field']);self.window.option_add('*TCombobox*Listbox.foreground',p['ink']);self.window.option_add('*TCombobox*Listbox.selectBackground',p['accent']);self.window.option_add('*TCombobox*Listbox.selectForeground',p['on_accent'])
  def walk(widget):
   kind=widget.winfo_class()
   if kind=='Text':
    note=getattr(widget,'_pcvr_note',False);muted=getattr(widget,'_pcvr_muted',False)
    widget.configure(bg=p['panel'] if note else p['field'],fg=p['muted'] if muted else p['ink'],font=('Consolas',base),selectbackground=p['accent'],selectforeground=p['on_accent'],insertbackground=p['accent'])
    if not note:widget.configure(highlightbackground=p['border'],highlightcolor=p['accent'])
    widget.tag_configure('warning',foreground=p['warning']);widget.tag_configure('success',foreground=p['accent']);widget.tag_configure('found',background=p['accent'],foreground=p['on_accent'])
   elif kind=='Listbox':widget.configure(bg=p['field'],fg=p['ink'],font=('Consolas',base),selectbackground=p['accent'],selectforeground=p['on_accent'],highlightbackground=p['border'])
   elif kind=='Menu':widget.configure(bg=p['panel'],fg=p['ink'],activebackground=p['accent'],activeforeground=p['on_accent'],font=('Consolas',base))
   elif kind=='Frame':widget.configure(bg=p['panel'])
   elif kind=='Scrollbar':widget.configure(bg=p['border'],activebackground=p['accent'],troughcolor=p['field'],highlightthickness=0)
   elif kind=='Canvas' and not getattr(widget,'_pcvr_background',False):
    widget.configure(bg=p['panel'])
    if getattr(widget,'_pcvr_iris',False):widget.itemconfigure('iris_even',fill=p['accent'],outline=p['panel']);widget.itemconfigure('iris_odd',fill=p['ink'],outline=p['panel'])
   for child in widget.winfo_children():walk(child)
  walk(self.window);self.paint()
 def controls(self,parent):
  from pcvr_terminal06 import note
  note(parent,'DISPLAY PREFERENCES\nChoose a light or dark chamber, an accent and a text size. Changes apply immediately and are remembered.')
  frame=self.ttk.Frame(parent);frame.pack(fill='x',pady=(8,20));frame.columnconfigure(1,weight=1)
  for row,(label,var,choices) in enumerate((('Display mode',self.mode,MODES),('Accent / terminal colour',self.accent,ACCENTS),('Text size',self.text_size,SIZES))):
   self.ttk.Label(frame,text=label).grid(row=row,column=0,sticky='w',padx=(0,24),pady=8)
   self.ttk.Combobox(frame,textvariable=var,values=tuple(choices),state='readonly',width=24).grid(row=row,column=1,sticky='ew',pady=8)

def setup(window,tk,ttk,root):
 controller=Controller(window,tk,ttk,root);window._pcvr_theme=controller;window.minsize(800,640)
 return dict(controller=controller,canvas=controller.canvas,enabled=controller.enabled,dimmer=controller.dimmer,photo=None,paint=controller.paint)
