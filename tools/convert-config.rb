require 'rio'

rio(ARGV[0]).each_line do |l|
  m= /#define\s+(.*)_checksum\s+CHECKSUM\((.*)\)/.match(l)

  if not m.nil?
    puts "#define #{m[1]}_key #{m[2]}"

  else
    # delta_e = config->value(delta_e_checksum)->by_default(131.636F)->as_number();
    if /config->value/.match(l)
      a= l.sub('config->value(', 'cr.get_XXX(m, ')
      a.sub!(')->by_default(', ', ')
      a.gsub!('_checksum', '_key')
      a.sub!('THEKERNEL->', '')
      m= /->as_(\w+)/.match(l)
      if not m.nil?
        s= case m[1]
        when "number"
          "float"
        when "bool"
          "bool"
        when "string"
          "string"
        end
        a.sub!('_XXX', "_#{s}")
        a.sub!("->as_#{m[1]}\(\)", "")
        puts a
      else
        puts a
      end
    else
      puts l
    end
  end

end
