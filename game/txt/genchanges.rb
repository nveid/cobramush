#!/usr/bin/ruby
#
# Synposis: Take files in changes directory and build the master changes.txt
#           and place the & help entry appropriately above the highest version
#           level 
#

$cur_version = "0.00"

# Simple add changes to it 
def chfname(filename) 
  return "changes/#{filename}"
end

def compare_versions(version)
  vers1 = $cur_version.split("p")
  vers2 = version.split("p")

  # First Check is.. See which base version is higher.. vers1 or vers2 is higher than the other.
  # quick way out & return that to $cur_version
  if vers1[0].to_f > vers2[0].to_f then
    return
  elsif vers2[0].to_f > vers1[0].to_f then
    $cur_version = version
  end

  # So we're in the same base vers level. compare the second patch operand

  if vers2[1] == nil then
    return
  end

  if vers1[1] == nil then
    $cur_version = version
    return
  end

  # Now do some actual comparisions between the two
  if vers1[1].to_i > vers2[1].to_i then
    return
  else
    $cur_version = version
  end
end

changes_files = Dir.entries "changes"

# First Lets find out which entry is gonna get the &help entry above it 
changes_files.each do |file|
    if FileTest.file?(chfname(file))  
     # Only recognize things that match x.yz?pN 
      if /^\d.\d\d(p\d)?$/ =~ file then
	compare_versions(file)
      end
    end
end

# Ok.. Nowe that we have cur_version.. Now we're gonna compile our master helpfile.
# Whenever we run across cur_version that is where we'll thrwo the & help
# entry right ontop
#

# Open our new changes.txt first

cfile = File.new("changes.txt", "w")
changes_files.each do  |file|
  if FileTest.file?(chfname(file)) 
    if $cur_version == file then
      cfile << "& help\n"
    end
    cfile << File.read(chfname(file))
  end
end

cfile.close
