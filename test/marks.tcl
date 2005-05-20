# The data for the chart
set data  {50 55 47 34 42 49 63 62 73 59 56 50 64
           60 67 67 58 59 73 77 84 82 80 84 98}

# The labels for the chart. Note the "-" means a minor tick.
set labels {"0\nJun 4" "-" "-" "3" "-" "-" "6" "-" 
            "-" "9" "-" "-" "12" "-" "-" "15" "-" "-" "18" "-" "-"
            "21" "-" "-" "0\nJun 5"}

# Create a XYChart object of size 400 x 270 pixels
set chart [ns_chartdir create xy 400 270]

# Set the plotarea at (80 25) and of size 300 x 200 pixels. Use
# alternate color background (0xe0e0e0) and (0xffffff). Set border and
# grid colors to grey (0xc0c0c0).
ns_chartdir setplotarea $chart 50 25 300 200 0xe0e0e0 0xffffff 0xc0c0c0 0xc0c0c0 0xc0c0c0

# Add a title to the chart using 14 pts Times Bold Italic font
ns_chartdir addtitle $chart "Server Monitor" Top "timesbi.ttf" 14

# Add a title to the y axis
ns_chartdir yaxis $chart settitle "Server Load (MBytes)"

# Set the y axis width to 2 pixels
ns_chartdir yaxis $chart setwidth 2

# Set the labels on the x axis
ns_chartdir xaxis $chart setlabels $labels

# Set the x axis width to 2 pixels
ns_chartdir xaxis $chart setwidth 2

# Add a horizontal red (0x800080) mark line at y  80
# Set the mark line width to 2 pixels
# Put the mark label at the top center of the mark line

ns_chartdir yaxis $chart addmark 80 0xff0000 2 "Critical Threshold Set Point"

# Add an orange (0xffcc66) zone from x  18 to x  20
ns_chartdir xaxis $chart addzone 18 20 0xffcc66

# Add a vertical brown (0x995500) mark line at x  18
# Set the mark line width to 2 pixels
# Put the mark label at the left of the mark line
# Rotate the mark label by 90 degrees so it draws vertically
ns_chartdir xaxis $chart addmark 18 0x995500 2 "Backup Start" Left "" 8 TextColor 90

# Add a vertical brown (0x995500) mark line at x  20
# Set the mark line width to 2 pixels
# Put the mark label at the right of the mark line
# Rotate the mark label by 90 degrees so it draws vertically
ns_chartdir xaxis $chart addmark 20 0x995500 2 "Backup End" Right "" 8 TextColor 90

# Add a green (0x00cc00) line layer with line width of 2 pixels
ns_chartdir layer $chart create line $data 0xcc00
ns_chartdir layer $chart setlinewidth 0 2

# output the chart in PNG format
ns_chartdir save $chart "marks.png"

set data [ns_chartdir image $chart]

# destroy the chart to free up resources
ns_chartdir destroy $chart
