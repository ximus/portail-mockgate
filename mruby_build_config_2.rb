MRuby::Build.new do |conf|
  toolchain :gcc

  enable_debug

  # include the default GEMs
  conf.gembox 'default'
end

MRuby::CrossBuild.new("edison") do |conf|
  toolchain :gcc

  conf.gembox 'default'

  conf.gem :git => 'https://github.com/matsumoto-r/mruby-sleep.git'
end