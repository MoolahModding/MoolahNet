#Py2Exe setup script
from distutils.core import setup
import py2exe

py2exe.freeze(
  console=['serverbrowser.py'],
  options={
    'py2exe': {
      'bundle_files': 1,
      'compressed': True
    }
  }
)