// .NET WebAssembly Runtime Loader
const dotnetRuntime = {
    // Runtime instance
    instance: null,
    
    // Configuration
    config: null,

    // Initialize the runtime
    async init(config) {
        this.config = config;
        
        // Load and instantiate the .NET runtime
        const response = await fetch('dotnet.wasm');
        const wasmBytes = await response.arrayBuffer();
        
        // Create import object for WASM
        const imports = {
            env: {
                // Memory management
                memory: new WebAssembly.Memory({
                    initial: config.heapSize / 64, // 64K pages
                    maximum: config.heapSize / 64,
                    shared: config.enableThreading
                }),
                
                // Console output
                'dotnet_console_log': function(ptr, len) {
                    const bytes = new Uint8Array(this.instance.exports.memory.buffer, ptr, len);
                    const text = new TextDecoder().decode(bytes);
                    console.log(text);
                },
                
                // File system operations
                'dotnet_read_file': async function(pathPtr, pathLen) {
                    const path = new TextDecoder().decode(
                        new Uint8Array(this.instance.exports.memory.buffer, pathPtr, pathLen)
                    );
                    
                    try {
                        const response = await fetch(path);
                        const data = await response.arrayBuffer();
                        
                        // Allocate memory for the file data
                        const ptr = this.instance.exports.malloc(data.byteLength);
                        new Uint8Array(this.instance.exports.memory.buffer).set(
                            new Uint8Array(data),
                            ptr
                        );
                        
                        return ptr;
                    } catch (error) {
                        console.error('Failed to read file:', path, error);
                        return 0;
                    }
                }
            }
        };
        
        // Instantiate WebAssembly module
        const wasmModule = await WebAssembly.instantiate(wasmBytes, imports);
        this.instance = wasmModule.instance;
        
        // Initialize the runtime
        const result = this.instance.exports.dotnet_init(
            config.enableThreading ? 1 : 0,
            config.enableAOT ? 1 : 0
        );
        
        if (result !== 0) {
            throw new Error('Failed to initialize .NET runtime');
        }
        
        // Load main assembly
        await this.loadMainAssembly();
    },
    
    // Load the main assembly
    async loadMainAssembly() {
        const assemblyPath = `${this.config.assemblyRoot}/${this.config.mainAssemblyName}`;
        const response = await fetch(assemblyPath);
        const assemblyData = await response.arrayBuffer();
        
        // Load the assembly into the runtime
        const dataPtr = this.instance.exports.malloc(assemblyData.byteLength);
        new Uint8Array(this.instance.exports.memory.buffer).set(
            new Uint8Array(assemblyData),
            dataPtr
        );
        
        const result = this.instance.exports.dotnet_load_assembly(
            dataPtr,
            assemblyData.byteLength
        );
        
        if (result !== 0) {
            throw new Error('Failed to load main assembly');
        }
        
        // Free the temporary buffer
        this.instance.exports.free(dataPtr);
    },
    
    // Call a method in the .NET runtime
    invokeMethod(typeName, methodName, ...args) {
        // Convert arguments to appropriate format
        const serializedArgs = JSON.stringify(args);
        const argsPtr = this.allocateString(serializedArgs);
        
        // Call the method
        const resultPtr = this.instance.exports.dotnet_invoke_method(
            this.allocateString(typeName),
            this.allocateString(methodName),
            argsPtr
        );
        
        // Get the result
        const result = this.readString(resultPtr);
        
        // Clean up
        this.instance.exports.free(argsPtr);
        
        return JSON.parse(result);
    },
    
    // Helper: Allocate a string in WASM memory
    allocateString(str) {
        const bytes = new TextEncoder().encode(str);
        const ptr = this.instance.exports.malloc(bytes.length + 1);
        new Uint8Array(this.instance.exports.memory.buffer).set(bytes, ptr);
        new Uint8Array(this.instance.exports.memory.buffer)[ptr + bytes.length] = 0; // Null terminator
        return ptr;
    },
    
    // Helper: Read a string from WASM memory
    readString(ptr) {
        if (ptr === 0) return null;
        
        const memory = new Uint8Array(this.instance.exports.memory.buffer);
        let end = ptr;
        while (memory[end] !== 0) end++;
        
        const bytes = memory.slice(ptr, end);
        return new TextDecoder().decode(bytes);
    }
};
