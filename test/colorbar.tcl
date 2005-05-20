# The data for the bar chart
set data {85 156 179.5 211 123}

# The labels for the bar chart
set labels {"Mon" "Tue" "Wed" "Thu" "Fri"}

# The colors for the bar chart
set colors {0xb8bc9c 0xa0bdc4 0x999966 0x333366 0xc3c3e6}

# Create a XYChart object of size 260 x 220 pixels
set chart [ns_chartdir create xy 260 220]

# Set the background color of the chart to gold (goldGradient). Use a 2
# pixel 3D border.
ns_chartdir setbackground $chart [ns_chartdir gradientcolor $chart goldGradient] -1 2

# Add a title box using 10 point Arial Bold font. Set the background
# color to blue metallic (blueMetalGradient). Use a 1 pixel 3D border.
ns_chartdir addtitle $chart "Daily Network Load" Top "arialbd.ttf" 10 \
            TextColor \
            [ns_chartdir gradientcolor $chart blueMetalGradient] -1 1

# Set the plotarea at (40 40) and of 200 x 150 pixels in size
ns_chartdir setplotarea $chart 40 40 200 150

# Add a multi-color bar chart layer using the given data and colors.
# Use a 1 pixel 3D border for the bars.
ns_chartdir layer $chart create bar $data "" $colors
ns_chartdir layer $chart setbordercolor 0 -1 1

# Set the x axis labels using the given labels
ns_chartdir xaxis $chart setlabels $labels

# output the chart in PNG format
ns_chartdir save $chart "colorbar.png"

# destroy the chart to free up resources
ns_chartdir destroy $chart
