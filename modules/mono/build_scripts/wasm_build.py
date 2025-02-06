#!/usr/bin/env python

import os
import subprocess
import shutil
import argparse
import json
from typing import List, Dict

def parse_args():
    parser = argparse.ArgumentParser(description='Build .NET assemblies for WebAssembly')
    parser.add_argument('--project-dir', required=True, help='The directory containing the .NET project')
    parser.add_argument('--output-dir', required=True, help='Output directory for the built files')
    parser.add_argument('--configuration', default='Release', help='Build configuration (Debug/Release)')
    parser.add_argument('--enable-threading', action='store_true', help='Enable threading support')
    parser.add_argument('--enable-aot', action='store_true', help='Enable ahead-of-time compilation')
    parser.add_argument('--heap-size', type=int, default=512, help='Heap size in MB')
    return parser.parse_args()

def ensure_dotnet_wasm_workload():
    """Ensure the .NET WebAssembly workload is installed."""
    try:
        subprocess.run(['dotnet', 'workload', 'install', 'wasm-tools'], check=True)
    except subprocess.CalledProcessError as e:
        print(f"Failed to install wasm-tools workload: {e}")
        raise

def build_project(args):
    """Build the .NET project for WebAssembly."""
    build_props: Dict[str, str] = {
        'Configuration': args.configuration,
        'RuntimeIdentifier': 'browser-wasm',
        'WasmEnableThreading': str(args.enable_threading).lower(),
        'WasmEnableExceptionHandling': 'true',
        'InvariantGlobalization': 'true',
        'EventSourceSupport': 'false',
        'UseSystemResourceKeys': 'true',
        'WasmHeapSize': str(args.heap_size * 1024 * 1024),  # Convert MB to bytes
    }

    if args.enable_aot:
        build_props.update({
            'RunAOTCompilation': 'true',
            'WasmStripILAfterAOT': 'true',
        })

    build_args = ['dotnet', 'publish']
    for key, value in build_props.items():
        build_args.extend(['-p:' + key + '=' + value])
    build_args.extend(['-o', args.output_dir])

    try:
        subprocess.run(build_args, cwd=args.project_dir, check=True)
    except subprocess.CalledProcessError as e:
        print(f"Build failed: {e}")
        raise

def copy_runtime_assets(args):
    """Copy necessary runtime assets to the output directory."""
    runtime_dir = os.path.join(args.output_dir, 'runtime')
    os.makedirs(runtime_dir, exist_ok=True)

    # Copy .NET runtime files
    dotnet_files = [
        'dotnet.wasm',
        'dotnet.js',
        'dotnet.timezones.blat',
    ]

    for file in dotnet_files:
        src = os.path.join(args.output_dir, file)
        dst = os.path.join(runtime_dir, file)
        if os.path.exists(src):
            shutil.copy2(src, dst)

def generate_assets_list(args):
    """Generate a list of assets that need to be loaded."""
    assets: List[Dict[str, str]] = []
    
    for root, _, files in os.walk(args.output_dir):
        for file in files:
            if file.endswith(('.dll', '.pdb', '.wasm')):
                rel_path = os.path.relpath(os.path.join(root, file), args.output_dir)
                assets.append({
                    'name': rel_path,
                    'path': rel_path,
                    'type': 'assembly' if file.endswith('.dll') else 'wasm'
                })

    assets_file = os.path.join(args.output_dir, 'assets.json')
    with open(assets_file, 'w') as f:
        json.dump(assets, f, indent=2)

def main():
    args = parse_args()

    print("Ensuring .NET WebAssembly workload is installed...")
    ensure_dotnet_wasm_workload()

    print(f"Building project for WebAssembly ({args.configuration})...")
    build_project(args)

    print("Copying runtime assets...")
    copy_runtime_assets(args)

    print("Generating assets list...")
    generate_assets_list(args)

    print("Build completed successfully!")

if __name__ == '__main__':
    main()
