# The data for the line chart
set data {50 55 47 34 42 49 63 62 73 59 56 50 64 60 
          67 67 58 59 73 77 84 82 80 91}

# The labels for the line chart
set labels {"Jan 2000" "Feb 2000" "Mar 2000" "Apr 2000"  
            "May 2000" "Jun 2000" "Jul 2000" "Aug 2000" "Sep 2000"
            "Oct 2000" "Nov 2000" "Dec 2000" "Jan 2001" "Feb 2001"
            "Mar 2001" "Apr 2001" "May 2001" "Jun 2001" "Jul 2001"
            "Aug 2001" "Sep 2001" "Oct 2001" "Nov 2001" "Dec 2001"}

# Create a XYChart object of size 500 x 320 pixels
set chart [ns_chartdir create xy 500 320]

# Set background color to pale purple 0xffccff, with a black edge and a
# 1 pixel 3D border
ns_chartdir setbackground $chart 0xffccff 0 1

# Set the plotarea at (55, 45) and of size 420 x 210 pixels, with white
# background. Turn on both horizontal and vertical grid lines with
# light grey color (0xc0c0c0)
ns_chartdir setplotarea $chart 55 45 420 210 0xffffff -1 -1 0xc0c0c0 -1

# Add a legend box at (55, 25) (top of the chart) with horizontal
# layout. Use 8 pts Arial font. Set the background and border color to
# Transparent.

ns_chartdir addlegend $chart 55 25 0 Transparent Transparent "" 8

# Add a title box to the chart using 13 pts Times Bold Italic font. The
# text is white (0xffffff) on a purple (0x800080) background, with a 1
# pixel 3D border.
ns_chartdir addtitle $chart "Long Term Server Load" Top "timesbi.ttf" 13 0xffffff 0x800080 -1 1

# Add a title to the y axis
ns_chartdir yaxis $chart settitle "MBytes"

# Set the labels on the x axis. Rotate the font by 90 degrees.
ns_chartdir xaxis $chart setlabels $labels
ns_chartdir xaxis $chart setlabelstyle "" 8 0xffff0002 90

# Add the data to the line layer using light brown color (0xcc9966)
# with a 7 pixel square symbol
ns_chartdir layer $chart create line $data "Server Utilization" 0xcc9966
ns_chartdir layer $chart setdatasymbol 0 0 SquareSymbol 7

# Set the line width to 3 pixels
ns_chartdir layer $chart setlinewidth 0 3

# Add a trend line layer using the same data with a dark green
# (0x008000) color. Set the line width to 3 pixels
ns_chartdir layer $chart create trend $data "Trend Line" 0x8000
ns_chartdir layer $chart setlinewidth 1 3

# output the chart in PNG format
ns_chartdir save $chart "trendline.png"

# destroy the chart to free up resources
ns_chartdir destroy $chart
