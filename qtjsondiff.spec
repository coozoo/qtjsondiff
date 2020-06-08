# spectool -g -R qtjsondiff.spec
# rpmbuild -ba qtjsondiff.spec

%define name QTjsonDiff
%define reponame qtjsondiff
%define version %(echo "$(curl --silent "https://raw.githubusercontent.com/coozoo/qtjsondiff/master/main.cpp"|grep "const QString APP"|awk -F\" '{print $2;}')")
%define build_timestamp %{lua: print(os.date("%Y%m%d"))}

Summary: QT json diff widget that consists of two json viewer widgets with highlighting of jsons. This app made as example of using widgets that can be integrated into your own Qt application but still it's pretty useful as such raw app.
Name: %{name}
Version: %{version}
Release: %{build_timestamp}
Source0: https://github.com/coozoo/qtjsondiff/archive/master.zip#/%{name}-%{version}-%{release}.tar.gz


License: MIT

BuildRequires: qt5-qtbase-devel >= 5.11
BuildRequires: qt5-linguist >= 5.11
BuildRequires: zlib-devel

# Requires: qt5 >= 5.11

Url: https://github.com/coozoo/qtjsondiff

%description

Some kind of diff viewer for Json (based on tree like json container/viewer widget).
Actually I've created this widget for myself. As tester often I need to compare JSONs from different sources or simply handy viewer which able to work sometimes with really big JSONs.
Usually online viewers are simply crashing and hanging with such data. This one viewer still able to work with such big JSONs.
For example I'm using this in my SignalR, cometD clients to visualize responses or simply to show http response and compare them from other sources. And found this example app pretty handy as well.

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
