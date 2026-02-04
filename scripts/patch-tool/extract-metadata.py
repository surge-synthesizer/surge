#!/usr/bin/env python3

import os
import struct
import csv
import xml.etree.ElementTree as ET

def extract_xml_from_fxp(filepath):
    """Extract XML content from an FXP file."""
    with open(filepath, mode='rb') as f:
        content = f.read()
    
    # Skip FXP header (60 bytes) and read patch header
    patch_header = struct.unpack("<4siiiiiii", content[60:92])
    xml_size = patch_header[1]
    xml_content = content[92:(92 + xml_size)].decode('utf-8')
    return xml_content

def parse_metadata(xml_content):
    """Parse metadata from XML content."""
    root = ET.fromstring(xml_content)
    meta = root.find('meta')
    if meta is None:
        return None
    
    return {
        'name': meta.get('name', ''),
        'category': meta.get('category', ''),
        'comment': meta.get('comment', ''),
        'author': meta.get('author', ''),
        'license': meta.get('license', '')
    }

def main():
    base_dir = os.path.join(os.path.dirname(__file__), '..', '..', 'resources', 'data')
    base_dir = os.path.abspath(base_dir)
    
    directories = ['patches_factory', 'patches_3rdparty']
    results = []
    
    for dir_name in directories:
        dir_path = os.path.join(base_dir, dir_name)
        if not os.path.exists(dir_path):
            continue
        
        for root, dirs, files in os.walk(dir_path):
            for filename in files:
                if filename.endswith('.fxp'):
                    filepath = os.path.join(root, filename)
                    rel_path = os.path.relpath(filepath, os.path.join(base_dir, '..', '..'))
                    
                    try:
                        xml_content = extract_xml_from_fxp(filepath)
                        metadata = parse_metadata(xml_content)
                        if metadata:
                            results.append({
                                'FileName': rel_path,
                                'Name': metadata['name'],
                                'Category': metadata['category'],
                                'Comment': metadata['comment'],
                                'Author': metadata['author'],
                                'License': metadata['license']
                            })
                    except Exception as e:
                        print(f"Error processing {filepath}: {e}")
    
    # Write CSV output
    output_path = os.path.join(os.path.dirname(__file__), '..', '..', 'patch-metadata.csv')
    output_path = os.path.abspath(output_path)
    
    with open(output_path, 'w', newline='') as csvfile:
        fieldnames = ['FileName', 'Name', 'Category', 'Comment', 'Author', 'License']
        writer = csv.DictWriter(csvfile, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(results)
    
    print(f"Written {len(results)} entries to {output_path}")

if __name__ == '__main__':
    main()
