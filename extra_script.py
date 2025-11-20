Import("env")
import os

def exclude_helium_files(node):
    """Filter out ARM Helium assembly files from LVGL"""
    return None if "helium" in node.get_path() else node

# Filter source files to exclude Helium
env.AddBuildMiddleware(exclude_helium_files, "*")
