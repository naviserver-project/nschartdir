# The data for the area chart
set data { 3.0 2.8 4.0 5.5 7.5 6.8 5.4 6.0 5.0 6.2 7.5 6.5 7.5 8.1 6.0 5.5 5.3 3.5 5.0 6.6 5.6 4.8 5.2 6.5 6.2}

# The labels for the area chart
set labels {"0" "" "" "3" "" "" "6" "" "" "9" "" "" "12" "" "" "15" "" "" "18" "" "" "21" "" "" "24"}

# Create a XYChart object of size 300 x 180 pixels. Set the background
# to pale yellow (0xffffa0) with a black border (0x0)
set chart [ns_chartdir create xy 300 180 0xffffa0 0x0]

# Set the plotarea at (45, 35) and of size 240 x 120 pixels. Set the
# background to white (0xffffff). Set both horizontal and vertical grid
# lines to black (&H0&) dotted lines (pattern code 0x0103)
ns_chartdir setplotarea $chart 45 35 240 120 0xffffff -1 -1 \
            [ns_chartdir dashlinecolor $chart 0x0 0x103] \
            [ns_chartdir dashlinecolor $chart 0x0 0x103]

# Add a title to the chart using 10 pts Arial Bold font. Use a 1 x 2
# bitmap pattern as the background. Set the border to black (0x0).
ns_chartdir addtitle $chart "Snow Percipitation (Dec 12)" Top "arialbd.ttf" 10 \
            TextColor \
            [ns_chartdir patterncolor $chart {0xb0b0f0 0xe0e0ff} 1 2] \
            Transparent \
            0x0

# Add a title to the y axis
ns_chartdir yaxis $chart settitle "mm per hour"

# Set the x axis labels using the given labels
ns_chartdir xaxis $chart setlabels $labels

# Add a data set to the area layer using the snow pattern as the fill
# color. Use deep blue (0x0000ff) as the area border line color
# (&H0000ff&)
ns_chartdir layer $chart create area $data
# Load a snow pattern from an external file "snow.png".
ns_chartdir layer $chart setdatacolor 0 0 [ns_chartdir patterncolor $chart "snow.png"] 0xff

# Set the line width to 2 pixels to highlight the line
ns_chartdir layer $chart setlinewidth 0 2

# output the chart in PNG format
ns_chartdir save $chart "linearea.png"

# destroy the chart to free up resources
ns_chartdir destroy $chart

