# The data for the line chart
set data0 {42 49 33 38 51 46 29 41 44 57 59 52 37 34 51 56 56 60 70 76 63 67 75 64 51}
set data1 {50 55 47 34 42 49 63 62 73 59 56 50 64 60 67 67 58 59 73 77 84 82 80 84 98}
set data2 {36 28 25 33 38 20 22 30 25 33 30 24 28 15 21 26 46 42 48 45 43 52 64 60 70}

# The labels for the line chart
set labels {"0" "" "" "3" "" "" "6" "" "" "9" "" "" "12" "" "" "15" "" "" "18" "" "" "21" "" "" "24"}

# Create a XYChart object of size 500 x 300 pixels
set chart [ns_chartdir create xy 500 300]

# Set background color to pale yellow 0xffff80, with a black edge and a
# 1 pixel 3D border
ns_chartdir setbackground $chart 0xffff80 0 1

# Set the plotarea at (55, 45) and of size 420 x 210 pixels, with white
# background. Turn on both horizontal and vertical grid lines with
# light grey color (0xc0c0c0)
ns_chartdir setplotarea $chart 55 45 420 210 0xffffff -1 -1 0xc0c0c0 -1

# Add a legend box at (55, 25) (top of the chart) with horizontal
# layout. Use 8 pts Arial font. Set the background and border color to
# Transparent.
ns_chartdir addlegend $chart 55 25 0 Transparent Transparent "" 8 TextColor

# Add a title box to the chart using 11 pts Arial Bold Italic font. The
# Add a title box to the chart using 11 pts Arial Bold Italic font. The
# text is white (0xffffff) on a dark red (0x800000) background, with a
# 1 pixel 3D border.
ns_chartdir addtitle $chart "Daily Server Load" Top "" 11 0xffffff 0x800000 -1 1

# Add a title to the y axis
ns_chartdir yaxis $chart settitle "MBytes"

# Set the labels on the x axis
ns_chartdir xaxis $chart setlabels $labels

# Add a title to the x axis
ns_chartdir xaxis $chart settitle "Jun 12, 2001"

# Add the three data sets to the line layer
ns_chartdir layer $chart create line $data0 "Server # 1"
ns_chartdir layer $chart dataset 0 $data1 "Server # 2"
ns_chartdir layer $chart dataset 0 $data2 "Server # 3"

# Set the default line width to 3 pixels
ns_chartdir layer $chart setlinewidth 0 3

# output the chart in PNG format
ns_chartdir save $chart "multiline.png"

# destroy the chart to free up resources
ns_chartdir destroy $chart

