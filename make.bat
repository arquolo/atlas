python setup.py build_ext --force -i
python setup.py sdist bdist_wheel
rem twine upload dist\*
