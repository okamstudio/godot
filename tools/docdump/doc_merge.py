import sys
import xml.etree.ElementTree as ET


tree = ET.parse(sys.argv[1])
old_doc=tree.getroot()

tree = ET.parse(sys.argv[2])
new_doc=tree.getroot()

f = file(sys.argv[3],"wb")
tab=0

old_classes={}

def write_string(_f, text,newline=True):
	for t in range(tab):
		_f.write("\t")
	_f.write(text)
	if (newline):
		_f.write("\n")

def escape(ret):
	ret=ret.replace("&","&amp;");
	ret=ret.replace("<","&gt;");
	ret=ret.replace(">","&lt;");
	ret=ret.replace("'","&apos;");
	ret=ret.replace("\"","&quot;");
	return ret
	

def inc_tab():
	global tab
	tab+=1

def dec_tab():
	global tab
	tab-=1

write_string(f,'<?xml version="1.0" encoding="UTF-8" ?>')
write_string(f,'<doc version="'+new_doc.attrib["version"]+'">')

def get_tag(node,name):
	tag=""
	if (name in node.attrib):
		tag=' '+name+'="'+escape(node.attrib[name])+'" '
	return tag

def find_method_descr(old_class,name):
	
	methods = old_class.find("methods")    
	if(methods!=None and len(list(methods))>0):
		for m in list(methods):
			if (m.attrib["name"]==name):
				description=m.find("description")
				if (description!=None and description.text.strip()!=""):
					return description.text
	
	return None

def find_signal_descr(old_class,name):
	
	signals = old_class.find("signals")    
	if(signals!=None and len(list(signals))>0):
		for m in list(signals):
			if (m.attrib["name"]==name):
				description=m.find("description")
				if (description!=None and description.text.strip()!=""):
					return description.text
	
	return None

def find_constant_descr(old_class,name):

	if (old_class==None):
		return None	
	constants = old_class.find("constants")    
	if(constants!=None and len(list(constants))>0):
		for m in list(constants):
			if (m.attrib["name"]==name):
				if (m.text.strip()!=""):
					return m.text
	return None

def write_class(c):
	class_name = c.attrib["name"]
	print("Parsing Class: "+class_name)
	if (class_name in old_classes):
		old_class=old_classes[class_name]
	else:
		old_class=None


	category=get_tag(c,"category")
	inherits=get_tag(c,"inherits")
	write_string(f,'<class name="'+class_name+'" '+category+inherits+'>')
	inc_tab()
	
	write_string(f,"<brief_description>")

	if (old_class!=None):
		old_brief_descr=old_class.find("brief_description")
		if (old_brief_descr!=None):
			write_string(f,escape(old_brief_descr.text.strip()))
		
	
	write_string(f,"</brief_description>")
	
	write_string(f,"<description>")
	if (old_class!=None):
		old_descr=old_class.find("description")
		if (old_descr!=None):
			write_string(f,escape(old_descr.text.strip()))
		
	write_string(f,"</description>")

	methods = c.find("methods")    
	if(methods!=None and len(list(methods))>0):

		write_string(f,"<methods>")
		inc_tab()
		
		for m in list(methods):
			qualifiers=get_tag(m,"qualifiers")

			write_string(f,'<method name="'+escape(m.attrib["name"])+'" ' +qualifiers+'>')
			inc_tab()

			for a in list(m):
				if (a.tag=="return"):
					typ=get_tag(a,"type")
					write_string(f,'<return'+typ+'>');
					write_string(f,'</return>');
				elif (a.tag=="argument"):
			
					default=get_tag(a,"default")

					write_string(f,'<argument index="'+a.attrib["index"]+'" name="'+escape(a.attrib["name"])+'" type="'+a.attrib["type"]+'"' +default+'>');
					write_string(f,'</argument>');

			write_string(f,'<description>');
			if (old_class!=None):
				old_method_descr=find_method_descr(old_class,m.attrib["name"])
				if (old_method_descr):
					write_string(f,escape(escape(old_method_descr.strip())))
			
			write_string(f,'</description>');
			dec_tab()					
			write_string(f,"</method>")
		dec_tab()
		write_string(f,"</methods>")

	signals = c.find("signals")    
	if(signals!=None and len(list(signals))>0):

		write_string(f,"<signals>")
		inc_tab()
		
		for m in list(signals):	

			write_string(f,'<signal name="'+escape(m.attrib["name"])+'">')
			inc_tab()

			for a in list(m):
				if (a.tag=="argument"):

					write_string(f,'<argument index="'+a.attrib["index"]+'" name="'+escape(a.attrib["name"])+'" type="'+a.attrib["type"]+'">');
					write_string(f,'</argument>');

			write_string(f,'<description>');
			if (old_class!=None):
				old_signal_descr=find_signal_descr(old_class,m.attrib["name"])
				if (old_signal_descr):
					write_string(f,escape(old_signal_descr.strip()))
			write_string(f,'</description>');
			dec_tab()					
			write_string(f,"</signal>")
		dec_tab()
		write_string(f,"</signals>")

	constants = c.find("constants")    
	if(constants!=None and len(list(constants))>0):

		write_string(f,"<constants>")
		inc_tab()
		
		for m in list(constants):	

			write_string(f,'<constant name="'+escape(m.attrib["name"])+'" value="'+m.attrib["value"]+'">')
			old_constant_descr=find_constant_descr(old_class,m.attrib["name"])
			if (old_constant_descr):
				write_string(f,escape(old_constant_descr.strip()))
			write_string(f,"</constant>")

		dec_tab()
		write_string(f,"</constants>")

	dec_tab()
	write_string(f,"</class>")

for c in list(old_doc):
	old_classes[c.attrib["name"]]=c

for c in list(new_doc):
	write_class(c)
write_string(f,'</doc>\n')

f.close()
