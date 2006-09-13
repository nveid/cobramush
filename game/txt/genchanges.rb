#!/usr/bin/ruby
#
# Synposis: Take files in changes directory and build the master changes.txt
#           and place the & help entry appropriately above the highest version
#           level 
#
#
class Array
  def tabulate(rowwidth=80,spacing=8)
    width = inject(0) { |m,i| [m,i.length].max } + spacing
    cols = rowwidth / width
    rows = [[]]
    each { |i|
      rows << [] if rows.last.size >= cols
      rows.last << i
    }
    rows.map { |i| i.map { |j| j.ljust(width) }.join.strip }
  end
end



$cur_version = "0.00"
entry_header = "-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-\n"


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

# Next Split our changes list into 2 seperate areas.. old & the new version schemes
newversions = Array.new
oldversions = Array.new
ni = 0
oi = 0
changes_files.each do |file|
    if FileTest.file?("changes/#{file}")  
      if /^\d.\d\d?[ab]?([p-]\d)?[ab]?$/ =~ file then
	newversions[ni] = file
	ni += 1
      else
	oldversions[oi] = file
	oi += 1
      end
    end
end
newversions = newversions.sort.reverse
oldversions = oldversions.sort.reverse

# Ok.. Nowe that we have cur_version.. Now we're gonna compile our master helpfile.
# Whenever we run across cur_version that is where we'll thrwo the & help
# entry right ontop
#

# Open our new changes.txt first

cfile = File.new("changes.txt", "w")
newversions.each do  |file|
  if $cur_version == file then
    cfile << "& help\n"
  end
  cfile << File.read(chfname(file))
end

oldversions.each do |file|
  cfile << File.read(chfname(file))
end

# close the file.. so.. we may reloop through this & build our entries ending thingie
cfile.close

# Now we're gonna reopen the changes file in append and read mode to add our entries
# entry
changes = File.open("changes.txt", "a+")
i = -1 
entries = Array.new


changes.each_line do |cur_line|
  if i == -1
    i = 0
  # This is where we open a new file.. a
  elsif cur_line[0,1] == "&" 
    curname = String.new(cur_line)
    curname[0] = " "
    curname = curname.strip
    entries[i] = curname
    i += 1
  end
end

changes << "& Entries\n"
changes << entry_header
line = 0
page = 1
entries.tabulate.each do |entry|
  line += 1
  if line == 9 then
    page += 1
    changes << "\n"
    changes << "More changes entries listed in entries#{page}".center(80)
    changes << "\n"
    changes << entry_header
    changes << "& Entries#{page}\n"
    changes << entry_header
    line = 0
  end
  changes << "#{entry}\n"
end

changes << entry_header
changes.close
