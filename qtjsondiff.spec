# spectool -g -R qtjsondiff.spec
# rpmbuild -ba qtjsondiff.spec

%define name QTjsonDiff
%define reponame qtjsondiff
%define version 0.33b
%define build_timestamp %{lua: print(os.date("%Y%m%d"))}

Summary: QT json diff widget that consists of two json viewer widgets with highlighting of jsons. This app made as example of using widgets that can be integrated into your own Qt application but still it's pretty useful as such raw app.
Name: %{name}
Version: %{version}
Release: %{build_timestamp}
Source0: https://github.com/coozoo/qtjsondiff/archive/master.zip#/%{name}-%{version}-%{release}.tar.gz


License: MIT


# Requires: qt5 >= 5.11

Url: https://github.com/coozoo/qtjsondiff

%description

Some kind of diff viewer for Json (based on tree like json container/viewer widget).
Actually I've created this widget for myself. As tester often I need to compare JSONs from different sources or simply handy viewer which able to work sometimes with really big JSONs.
Usually online viewers are simply crashing and hanging with such data. This one viewer still able to work with such big JSONs.
For example I'm using this in my SignalR, cometD clients to visualize responses or simply to show http response and compare them from other sources. And found this example app pretty handy as well.

Some features:
- two view modes json formatted text or json tree (switch by "Show JSON Text/View" button);
- load json from file, url or copy paste (CTRL+V in treeview mode to paste JSON);
- search through json text or json model (backward, forward, casesensitivity);
- compare two jsons with highlightings, sync scrolling, sync item selection (only treeview mode);
- two comparison modes (switched by "Use Full Path" checkbox):
  * follow by full path;
  * try to find child+parent pair anywhere inside JSON (first occurrence).
- copy text into clipboard:
  * **Copy Row** - item text "Name Type Value" separated by tab;
  * **Copy Rows** - all items text separated by tab (tabs allow spreadsheet paste);
  * **Copy Path** - path to the item in such format "name(type)->name(type)"
       For example: root(Object)->widget(Object)->image(Object)->alignment(String);
  * **Copy Plain Json** - copy full plain text JSON (not formatted);
  * **Copy Pretty Json** - copy full pretty print JSON;
  * **Copy Selected Json Value** - copy value, array or object.

%global debug_package %{nil}

%prep
%setup -q -n %{name}-%{version}

%build
# don't know maybe it's stupid me but lrelease in qt looks like runs after make file generation as result automatic file list inside qmake doesn't work
# so what I need just run it twice...
qmake-qt5
make
qmake-qt5
make

%install
make INSTALL_ROOT=%{buildroot} -j$(nproc) install

%post

%postun

%files
%{_bindir}/*
%{_datadir}/*

%changelog
