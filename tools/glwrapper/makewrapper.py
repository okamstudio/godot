#! /usr/bin/python
import sys

if (len(sys.argv)<2):
  print("usage: makewrapper.py <headers>")
  sys.exit(255)
  

functions=[]
types=[]
constants=[]
  
READ_FUNCTIONS=0
READ_TYPES=1
READ_CONSTANTS=2
  
read_what=READ_TYPES
  
for x in (range(len(sys.argv)-1)):
  f=open(sys.argv[x+1],"r")
  
  while(True):
  
    line=f.readline()
    if (line==""):
      break
      
    line=line.replace("\n","").strip()
    """
    if (line.find("[types]")!=-1):
      read_what=READ_TYPES
      continue
    elif (line.find("[constants]")!=-1):
      read=READ_TYPES
      continue
    elif (line.find("[functions]")!=-1):
      read_what=READ_FUNCTIONS
      continue
     """
     
    if (line.find("#define")!=-1):
      if (line.find("0x")==-1 and line.find("GL_VERSION")==-1):
        continue
      constants.append(line)
    elif (line.find("typedef")!=-1):
      if (line.find("(")!=-1 or line.find(")")!=-1 or line.find("ARB")!=-1 or line.find("EXT")!=-1 or line.find("GL")==-1):
        continue
      types.append(line)
    elif (line.find("APIENTRY")!=-1 and line.find("GLAPI")!=-1):

      if (line.find("ARB")!=-1 or line.find("EXT")!=-1 or line.find("NV")!=-1):
        continue

      line=line.replace("APIENTRY","")   
      line=line.replace("GLAPI","")   
      
      glpos=line.find(" gl")
      if (glpos==-1):
      
       glpos=line.find("\tgl")
       if (glpos==-1):
          continue
   
      ret=line[:glpos].strip();
      
      line=line[glpos:].strip()
      namepos=line.find("(")
      
      if (namepos==-1):
        continue
        
      name=line[:namepos].strip()
      line=line[namepos:]
      
      argpos=line.rfind(")")
      if (argpos==-1):
        continue
        
      args=line[1:argpos]
      
      funcdata={}
      funcdata["ret"]=ret
      funcdata["name"]=name
      funcdata["args"]=args
      
      functions.append(funcdata)
      print(funcdata)
            
  
    
#print(types)
#print(constants)
#print(functions)
      
    
f=open("glwrapper.h","w")

f.write("#ifndef GL_WRAPPER\n")
f.write("#define GL_WRAPPER\n\n\n")

header_code="""\
#if defined(__gl_h_) || defined(__GL_H__)
#error gl.h included before glwrapper.h
#endif
#if defined(__glext_h_) || defined(__GLEXT_H_)
#error glext.h included before glwrapper.h
#endif
#if defined(__gl_ATI_h_)
#error glATI.h included before glwrapper.h
#endif

#define __gl_h_    
#define __GL_H__   
#define __glext_h_ 
#define __GLEXT_H_ 
#define __gl_ATI_h_

#define GL_TRUE 1
#define GL_FALSE 0

#define GL_ZERO                           0
#define GL_ONE                            1
#define GL_NONE                           0
#define GL_NO_ERROR                       0

\n\n
"""

f.write("#include <stddef.h>\n\n\n")

f.write(header_code);

f.write("#ifdef __cplusplus\nextern \"C\" {\n#endif\n\n\n")
f.write("#if defined(_WIN32) && !defined(__CYGWIN__)\n")
f.write("#define GLWRP_APIENTRY __stdcall\n")
f.write("#else\n");
f.write("#define GLWRP_APIENTRY \n")
f.write("#endif\n\n");
for x in types:
  f.write(x+"\n")
  
f.write("\n\n")

for x in constants:
  f.write(x+"\n")

f.write("\n\n")

for x in functions:
  f.write("extern "+x["ret"]+" GLWRP_APIENTRY (*__wrapper_"+x["name"]+")("+x["args"]+");\n")
  f.write("#define "+x["name"]+" __wrapper_"+x["name"]+"\n")

f.write("\n\n")
f.write("typedef void (*GLWrapperFuncPtr)(void);\n\n");
f.write("void glWrapperInit( GLWrapperFuncPtr (*wrapperFunc)(const char*) );\n")
  
f.write("#ifdef __cplusplus\n}\n#endif\n")

f.write("#endif\n\n")
f.close()
  
f=open("glwrapper.c","w")

f.write("\n\n")
f.write("#include \"glwrapper.h\"\n")
f.write("\n\n")
  
for x in functions:
  f.write(x["ret"]+" GLWRP_APIENTRY (*__wrapper_"+x["name"]+")("+x["args"]+")=NULL;\n")
  
f.write("\n\n")
f.write("void glWrapperInit( GLWrapperFuncPtr (*wrapperFunc)(const char*) )  {\n")
f.write("\n")
  
for x in functions:
  f.write("\t__wrapper_"+x["name"]+"=("+x["ret"]+" GLWRP_APIENTRY (*)("+x["args"]+"))wrapperFunc(\""+x["name"]+"\");\n")

f.write("\n\n")
f.write("}\n")
f.write("\n\n")
f.close()

      
    
