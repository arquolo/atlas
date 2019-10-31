python setup.py build_ext --force -i
python setup.py sdist bdist_wheel
REM twine upload dist\*
