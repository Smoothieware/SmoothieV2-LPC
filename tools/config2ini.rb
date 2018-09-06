require 'inifile'
require 'rio'


hash= {'general' => {} }

rio(ARGV[0]).each_line do |l|
  next if l.start_with?('#')

  a= l.split(' ')
  if a.size < 2
    next
  end

  k= a[0].strip
  c= k.split('.')
  v= a[1..-1].join(' ')

  if c.size == 1
    hash['general'].store(c[0], v)

  else
    hash[c[0]] ||= {}
    hash[c[0]].store(c[1..-1].join('.'), v)
  end

end


ini_file = IniFile.new(:content => hash)

p ini_file

ini_file.save(:filename => 'test.ini')
