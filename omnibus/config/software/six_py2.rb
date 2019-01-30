name "six_py2"
default_version "1.11.0"

dependency "pip"

build do
  ship_license "https://bitbucket.org/gutworth/six/raw/e5218c3f66a2614acb7572204a27e2b508682168/LICENSE"
  py2pip "install #{name}==#{version}"
end
