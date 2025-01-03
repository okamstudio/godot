import sys
import xml.etree.ElementTree as ET

input_list = []


for arg in sys.argv[1:]:
  input_list.append(arg)

if len(input_list) < 1:
  print("usage: makedoku.py <class_list.xml>")
  sys.exit(0)


def validate_tag(elem,tag):
  if (elem.tag != tag):
    print("Tag mismatch, expected '"+tag+"', got "+elem.tag);
    sys.exit(255)
 

class_names=[]
classes={}


def make_class_list(class_list,columns):
	
	f=open("class_list.txt","wb")
	prev=0
	col_max = len(class_list) / columns + 1
	print("col max is ", col_max)
	col_count = 0
	row_count = 0
	last_initial = ""
	fit_columns=[]
  
	for n in range(0,columns):
		fit_columns+=[[]]

	indexers=[]
	last_initial=""
     
	idx=0
	for n in class_list:
		col = idx/col_max
		if (col>=columns):
			col=columns-1
		fit_columns[col]+=[n]
		idx+=1
		if (n[:1]!=last_initial):
			indexers+=[n]
		last_initial=n[:1]
		

	row_max=0
  
	for n in range(0,columns):
		if (len(fit_columns[n])>row_max):
			row_max=len(fit_columns[n])
    

	for r in range(0,row_max):
		s="|"
		for c in range(0,columns):   
			if (r>=len(fit_columns[c])):
				continue
    
			classname = fit_columns[c][r]
			initial=classname[0]
			if (classname in indexers):
				s+="**"+initial+"**|"
			else:
				s+=" |"
				
			s+="[["+classname.lower()+"|"+classname+"]]|"
			
		s+="\n"
		f.write(s)
	f.close()


def dokuize_text(txt):
	
	return txt


def dokuize_text(text):
 pos=0
 while(True):
  pos = text.find("[",pos)
  if (pos==-1):
   break

  endq_pos=text.find("]",pos+1)
  if (endq_pos==-1):
   break
    
  pre_text=text[:pos]
  post_text=text[endq_pos+1:]
  tag_text=text[pos+1:endq_pos]

  if (tag_text in class_names):
    tag_text="[["+tag_text.lower()+"|"+tag_text+"]]"   
  else: #command
   cmd=tag_text
   space_pos=tag_text.find(" ")
   if (cmd.find("html")==0):
    cmd=tag_text[:space_pos]
    param=tag_text[space_pos+1:]
    tag_text="<"+param+">"
   elif(cmd.find("method")==0):
    cmd=tag_text[:space_pos]
    param=tag_text[space_pos+1:]
    
    if (param.find(".")!=-1):
     class_param,method_param=param.split(".")
     tag_text="[["+class_param.lower()+"#"+method_param+"|"+class_param+'.'+method_param+"]]"
    else:
     tag_text="[[#"+param+"|"+param+"]]"
   elif (cmd.find("image=")==0):
    tag_text="{{"+cmd[6:]+"}}"
   elif (cmd.find("url=")==0):
    tag_text="[["+cmd[4:]+"|"
   elif (cmd=="/url"):
    tag_text="]]>" 
   elif (cmd=="center"):
    tag_text=""
   elif (cmd=="/center"):
    tag_text=""
   elif (cmd=="br"):
    tag_text="\\\\\n"
   elif (cmd=="i" or cmd=="/i"):
    tag_text="//"
   elif (cmd=="b" or cmd=="/b"):
    tag_text="**"
   elif (cmd=="u" or cmd=="/u"):
    tag_text="__"
   else:
    tag_text="["+tag_text+"]"
   
 
  text=pre_text+tag_text+post_text
  pos=len(pre_text)+len(tag_text)
  
 #tnode = ET.SubElement(parent,"div")
 #tnode.text=text
 return text
 	
	
def make_type(t):
	global class_names
	if (t in class_names):
		return "[["+t.lower()+"|"+t+"]]"
	return t
	
	
def make_method(f,name,m,declare,event=False):
	
	s="  * "
	ret_type="void"
	args=list(m)
	mdata={}
	mdata["argidx"]=[]
	for a in args:
		if (a.tag=="return"):
			idx=-1
		elif (a.tag=="argument"):
			idx=int(a.attrib["index"])
		else:
			continue
   
		mdata["argidx"].append(idx)
		mdata[idx]=a
		
	

	if (not event):  
		if (-1 in mdata["argidx"]):
			s+=make_type(mdata[-1].attrib["type"])
		else: 
			s+="void"
		s+=" "
		
	if (declare):
		
		#span.attrib["class"]="funcdecl"
		#a=ET.SubElement(span,"a")
		#a.attrib["name"]=name+"_"+m.attrib["name"]
		#a.text=name+"::"+m.attrib["name"]
		s+="**"+m.attrib["name"]+"**"
	else:		
		s+="[[#"+m.attrib["name"]+"|"+m.attrib["name"]+"]]"
		
	s+="**(**"
	argfound=False
	for a in mdata["argidx"]:
		arg=mdata[a]
		if (a<0):
			continue
		if (a>0):  
			s+=", "
		else:
			s+=" "
		
		s+=make_type(arg.attrib["type"])
		if ("name" in arg.attrib):
			s+=" "+arg.attrib["name"]
		else:
			s+=" arg"+str(a)
		
		if ("default" in arg.attrib):
			s+="="+arg.attrib["default"]
		
		
		argfound=True
		
	if (argfound):
		s+=" "
	s+="**)**"
		
	if ("qualifiers" in m.attrib):
		s+=" "+m.attrib["qualifiers"]
		
	f.write(s+"\n")
	f.close()
  
def make_doku_class(node):

	name = node.attrib["name"]
	
	f=open(name.lower()+".txt","wb")

	f.write("======  "+name+"  ======\n")

	if ("inherits" in node.attrib):
		inh=node.attrib["inherits"].strip()
		f.write("**Inherits:** [["+inh.lower()+"|"+inh+"]]\\\\\n")
	if ("category" in node.attrib):
		f.write("**Category:** "+node.attrib["category"].strip()+"\\\\\n")
	
	briefd = node.find("brief_description")
	if (briefd!=None):
		f.write("=====  Brief Description  ======\n")
		f.write( dokuize_text(briefd.text.strip())+"\n" )
	
	methods = node.find("methods")  
  
	if(methods!=None and len(list(methods))>0):
		f.write("=====  Member Functions  ======\n")
		for m in list(methods):
			make_method(f,node.attrib["name"],m,False)
		
	events = node.find("signals")  
	if(events!=None and len(list(events))>0):	
		f.write("=====  Signals  ======\n")
		for m in list(events):
			make_method(f,node.attrib["name"],m,True,True)
			
	members = node.find("members")  

	if(members!=None and len(list(members))>0):
		f.write("=====  Member Variables  ======\n")

		for c in list(members):
			s = "  * "
			s+=make_type(c.attrib["type"])+" "
			s+="**"+c.attrib["name"]+"**"
			if (c.text.strip()!=""):
				s+=" - "+c.text.strip()
			f.write(s+"\n")
				
	
			
	constants = node.find("constants")  
	if(constants!=None and len(list(constants))>0):
		f.write("=====  Numeric Constants  ======\n")
		for c in list(constants):
			s = "  * "
			s+="**"+c.attrib["name"]+"**"
			if ("value" in c.attrib):
				s+=" = **"+c.attrib["value"]+"**"			
			if (c.text.strip()!=""):
				s+=" - "+c.text.strip()
			f.write(s+"\n")
			
		
	descr=node.find("description")
	if (descr!=None and descr.text.strip()!=""):
		f.write("=====  Description  ======\n")
		f.write(dokuize_text(descr.text.strip())+"\n")

	methods = node.find("methods")  
		
	if(methods!=None and len(list(methods))>0):
		f.write("=====  Member Function Description  ======\n")
		for m in list(methods):
			
			d=m.find("description")
			if (d==None or d.text.strip()==""):
				continue
			f.write("==  "+m.attrib["name"]+"  ==\n")
			make_method(f,node.attrib["name"],m,False)
			f.write("\\\\\n")
			f.write(dokuize_text(d.text.strip()))
			f.write("\n")
			
	f.close()

			
	"""
  div=ET.Element("div")
  div.attrib["class"]="class";

  a=ET.SubElement(div,"a")
  a.attrib["name"]=node.attrib["name"]
  
  h3=ET.SubElement(a,"h3")
  h3.attrib["class"]="title class_title"
  h3.text=node.attrib["name"]

  briefd = node.find("brief_description")
  if (briefd!=None):
   div2=ET.SubElement(div,"div")
   div2.attrib["class"]="description class_description"
   div2.text=briefd.text
   
  if ("inherits" in node.attrib):
   ET.SubElement(div,"br")

   div2=ET.SubElement(div,"div")   
   div2.attrib["class"]="inheritance";

   span=ET.SubElement(div2,"span")
   span.text="Inherits: "
   
   make_type(node.attrib["inherits"],div2)
   
  if ("category" in node.attrib):
   ET.SubElement(div,"br")
   
   div3=ET.SubElement(div,"div")   
   div3.attrib["class"]="category";
    
   span=ET.SubElement(div3,"span")
   span.attrib["class"]="category"
   span.text="Category: "
    
   a = ET.SubElement(div3,"a")
   a.attrib["class"]="category_ref"
   a.text=node.attrib["category"]
   catname=a.text
   if (catname.rfind("/")!=-1):
    catname=catname[catname.rfind("/"):] 
   catname="CATEGORY_"+catname
    
   if (single_page):
    a.attrib["href"]="#"+catname
   else:
    a.attrib["href"]="category.html#"+catname
    
    
  methods = node.find("methods")  
  
  if(methods!=None and len(list(methods))>0):

   h4=ET.SubElement(div,"h4")
   h4.text="Public Methods:"

   method_table=ET.SubElement(div,"table")   
   method_table.attrib["class"]="method_list";

   for m in list(methods):
#    li = ET.SubElement(div2, "li")
    method_table.append( make_method_def(node.attrib["name"],m,False) )

  events = node.find("signals")  
  
  if(events!=None and len(list(events))>0):
   h4=ET.SubElement(div,"h4")
   h4.text="Events:"

   event_table=ET.SubElement(div,"table")   
   event_table.attrib["class"]="method_list";

   for m in list(events):
#    li = ET.SubElement(div2, "li")
    event_table.append( make_method_def(node.attrib["name"],m,False,True) )


  members = node.find("members")  
  if(members!=None and len(list(members))>0):

   h4=ET.SubElement(div,"h4")
   h4.text="Public Variables:"
   div2=ET.SubElement(div,"div")   
   div2.attrib["class"]="member_list";
   
   for c in list(members):
   
    li = ET.SubElement(div2, "li")
    div3=ET.SubElement(li,"div")
    div3.attrib["class"]="member";
    make_type(c.attrib["type"],div3)    
    span=ET.SubElement(div3,"span")
    span.attrib["class"]="identifier member_name"
    span.text=" "+c.attrib["name"]+" "
    span=ET.SubElement(div3,"span")
    span.attrib["class"]="member_description"
    span.text=c.text


  constants = node.find("constants")  
  if(constants!=None and len(list(constants))>0):

   h4=ET.SubElement(div,"h4")
   h4.text="Constants:"
   div2=ET.SubElement(div,"div")   
   div2.attrib["class"]="constant_list";
   
   for c in list(constants):
    li = ET.SubElement(div2, "li")
    div3=ET.SubElement(li,"div")
    div3.attrib["class"]="constant";
    
    span=ET.SubElement(div3,"span")
    span.attrib["class"]="identifier constant_name"
    span.text=c.attrib["name"]+" "
    if ("value" in c.attrib):
     span=ET.SubElement(div3,"span")
     span.attrib["class"]="symbol"
     span.text="= "
     span=ET.SubElement(div3,"span")
     span.attrib["class"]="constant_value"
     span.text=c.attrib["value"]+" "
    span=ET.SubElement(div3,"span")
    span.attrib["class"]="constant_description"
    span.text=c.text
     
#  ET.SubElement(div,"br")
   
  
  descr=node.find("description")
  if (descr!=None and descr.text.strip()!=""):

   h4=ET.SubElement(div,"h4")
   h4.text="Description:"
  
   make_text_def(node.attrib["name"],div,descr.text)
#   div2=ET.SubElement(div,"div")   
#   div2.attrib["class"]="description";
#   div2.text=descr.text
  
  
  
  if(methods!=None or events!=None):

   h4=ET.SubElement(div,"h4")
   h4.text="Method Documentation:"
   iter_list = []
   if (methods!=None):
    iter_list+=list(methods)
   if (events!=None):
    iter_list+=list(events)

   for m in iter_list:

    descr=m.find("description")
                 
    if (descr==None or descr.text.strip()==""):
     continue;

    div2=ET.SubElement(div,"div")
    div2.attrib["class"]="method_doc";


    div2.append( make_method_def(node.attrib["name"],m,True) )
	#anchor = ET.SubElement(div2, "a")
	#anchor.attrib["name"] =
    make_text_def(node.attrib["name"],div2,descr.text)
    #div3=ET.SubElement(div2,"div")   
    #div3.attrib["class"]="description";
    #div3.text=descr.text
     
  
  return div
"""
for file in input_list:
	tree = ET.parse(file)
	doc=tree.getroot()

	if ("version" not in doc.attrib):
		print("Version missing from 'doc'")
		sys.exit(255)

	version=doc.attrib["version"]

	for c in list(doc):
		if (c.attrib["name"] in class_names):
			continue
		class_names.append(c.attrib["name"])
		classes[c.attrib["name"]]=c
		

class_names.sort()
  
make_class_list(class_names,4)

for cn in class_names:
	c=classes[cn]
	make_doku_class(c)
 

