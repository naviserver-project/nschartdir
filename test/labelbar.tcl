# The data for the bar chart
set data0 {100 125 245 147 67}
set data1 {85 156 179 211 123}
set data2 {97 87 56 267 157}
        
# The labels for the bar chart
set labels {"Mon" "Tue" "Wed" "Thu" "Fri"}

# Create a XYChart object of size 500 x 320 pixels
set chart [ns_chartdir create xy 500 320]

# Set the plotarea at (100 40) and of size 280 x 240 pixels
ns_chartdir setplotarea $chart 100 40 280 240

# Add a legend box at (400 100)
ns_chartdir addlegend $chart 400 100
    
# Add a title to the chart using 14 points Times Bold Itatic font
ns_chartdir addtitle $chart "Weekday Network Load" Top "timesbi.ttf" 14
    
# Add a title to the y axis. Draw the title upright (font angle  0)
ns_chartdir yaxis $chart settitle "Average\nWorkload\n(MBytes\nPer Hour)"

# Set the labels on the x axis
ns_chartdir xaxis $chart setlabels $labels

# Add a stacked bar layer and set the layer 3D depth to 8 pixels
# Add the three data sets to the bar layer
ns_chartdir layer $chart create bar $data0 "Server # 1"
ns_chartdir layer $chart setdatacombinemethod 0 Stack
ns_chartdir layer $chart setdepth 0 8 8
ns_chartdir layer $chart dataset 0 $data1 "Server # 2"
ns_chartdir layer $chart dataset 0 $data2 "Server # 3"

# Enable bar label for the whole bar
ns_chartdir layer $chart setaggregatelabelstyle 0 ""

# Enable bar label for each segment of the stacked bar
ns_chartdir layer $chart setdatalabelstyle 0 ""

# Reserve 10% margin at the top of the plot area during auto-scaling.
# This ensures the data labels will not fall outside the plot area.
ns_chartdir yaxis $chart setautoscale 0.1

# output the chart in PNG format
ns_chartdir save $chart "labelbar.png"

# destroy the chart to free up resources
ns_chartdir destroy $chart
